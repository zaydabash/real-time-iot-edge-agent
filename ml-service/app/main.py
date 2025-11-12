"""
FastAPI service for Isolation Forest anomaly detection
"""

import os
import pickle
from typing import List, Optional
from datetime import datetime
import numpy as np
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(title="IoT Anomaly Detection ML Service")

# Configuration from environment
ISOFOREST_WINDOW = int(os.getenv("ISOFOREST_WINDOW", "512"))
ISOFOREST_CONTAM = float(os.getenv("ISOFOREST_CONTAM", "0.03"))
ISOFOREST_THRESHOLD = float(os.getenv("ISOFOREST_THRESHOLD", "0.65"))
WARM_START = os.getenv("WARM_START", "true").lower() == "true"

# Per-device models storage
models: dict[str, IsolationForest] = {}
scalers: dict[str, StandardScaler] = {}
device_windows: dict[str, List[dict]] = {}


class MetricPoint(BaseModel):
    ts: str
    temperature_c: float
    vibration_g: float
    humidity_pct: float
    voltage_v: float


class ScoreBatchRequest(BaseModel):
    deviceId: str
    points: List[MetricPoint]


class ScoredPoint(BaseModel):
    index: int
    score: float
    isAnomaly: bool


class ScoreBatchResponse(BaseModel):
    scores: List[ScoredPoint]


def extract_features(points: List[MetricPoint]) -> np.ndarray:
    """Extract feature matrix from metric points"""
    features = []
    for point in points:
        features.append([
            point.temperature_c,
            point.vibration_g,
            point.humidity_pct,
            point.voltage_v
        ])
    return np.array(features)


def get_or_create_model(device_id: str) -> tuple[IsolationForest, StandardScaler]:
    """Get existing model or create new one for device"""
    if device_id in models and WARM_START:
        return models[device_id], scalers[device_id]
    
    # Create new model
    model = IsolationForest(
        n_estimators=100,
        contamination=ISOFOREST_CONTAM,
        random_state=42,
        warm_start=WARM_START
    )
    scaler = StandardScaler()
    
    models[device_id] = model
    scalers[device_id] = scaler
    
    logger.info(f"Created new model for device {device_id}")
    return model, scaler


def update_model_window(device_id: str, points: List[MetricPoint]):
    """Maintain sliding window for device"""
    if device_id not in device_windows:
        device_windows[device_id] = []
    
    window = device_windows[device_id]
    window.extend([p.dict() for p in points])
    
    # Keep only last N points
    if len(window) > ISOFOREST_WINDOW:
        device_windows[device_id] = window[-ISOFOREST_WINDOW:]


@app.get("/health")
async def health():
    """Health check endpoint"""
    return {"ok": True, "service": "ml-anomaly-detection"}


@app.post("/score-batch", response_model=ScoreBatchResponse)
async def score_batch(request: ScoreBatchRequest):
    """
    Score a batch of metric points for anomalies using Isolation Forest
    """
    try:
        device_id = request.deviceId
        points = request.points
        
        if not points:
            raise HTTPException(status_code=400, detail="No points provided")
        
        # Get or create model for device
        model, scaler = get_or_create_model(device_id)
        
        # Extract features
        features = extract_features(points)
        
        # Update window
        update_model_window(device_id, points)
        
        # Get training window
        window = device_windows[device_id]
        if len(window) < 10:
            # Not enough data, return neutral scores
            scores = [ScoredPoint(index=i, score=0.5, isAnomaly=False) 
                     for i in range(len(points))]
            return ScoreBatchResponse(scores=scores)
        
        # Prepare training data
        train_features = extract_features([MetricPoint(**p) for p in window])
        
        # Fit scaler and model if needed
        if not hasattr(model, 'estimators_') or len(model.estimators_) == 0:
            train_scaled = scaler.fit_transform(train_features)
            model.fit(train_scaled)
        else:
            # Partial fit if warm start enabled
            train_scaled = scaler.transform(train_features)
            if WARM_START and hasattr(model, 'partial_fit'):
                try:
                    model.partial_fit(train_scaled)
                except:
                    # Fallback to full fit
                    model.fit(train_scaled)
        
        # Scale input features
        input_scaled = scaler.transform(features)
        
        # Predict anomalies (returns -1 for anomaly, 1 for normal)
        predictions = model.predict(input_scaled)
        
        # Get anomaly scores (lower = more anomalous)
        scores_raw = model.score_samples(input_scaled)
        
        # Normalize scores to [0, 1] range (invert so higher = more anomalous)
        scores_normalized = 1.0 / (1.0 + np.exp(-scores_raw))
        
        # Build response
        scored_points = []
        for i, (pred, score) in enumerate(zip(predictions, scores_normalized)):
            is_anomaly = pred == -1 or score > ISOFOREST_THRESHOLD
            scored_points.append(ScoredPoint(
                index=i,
                score=float(score),
                isAnomaly=bool(is_anomaly)
            ))
        
        logger.info(f"Scored {len(points)} points for device {device_id}: "
                   f"{sum(1 for s in scored_points if s.isAnomaly)} anomalies")
        
        return ScoreBatchResponse(scores=scored_points)
        
    except Exception as e:
        logger.error(f"Error scoring batch: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))


if __name__ == "__main__":
    import uvicorn
    port = int(os.getenv("PORT", "8000"))
    uvicorn.run(app, host="0.0.0.0", port=port)

