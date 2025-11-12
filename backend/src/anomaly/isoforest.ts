/**
 * Isolation Forest Anomaly Detection Engine
 * 
 * Uses a simplified isolation forest algorithm to detect anomalies
 * in multi-dimensional metric space.
 * 
 * Note: This is a simplified implementation. For production use,
 * consider using a more robust library like ml-isolation-forest
 * or implementing a full isolation forest algorithm.
 */

import { AnomalyEngine, AnomalyResult, MetricPoint } from './engine';

// Simplified Isolation Forest implementation
class SimpleIsolationForest {
  private contamination: number;
  private nEstimators: number;
  private maxSamples: number;

  constructor(contamination: number = 0.1, nEstimators: number = 100, maxSamples: number = 256) {
    this.contamination = contamination;
    this.nEstimators = nEstimators;
    this.maxSamples = maxSamples;
  }

  // Simplified anomaly score based on distance from median
  score(features: number[][]): number[] {
    if (features.length === 0) return [];

    const nFeatures = features[0].length;
    const medians: number[] = [];
    const mads: number[] = []; // Median Absolute Deviation

    // Calculate median and MAD for each feature
    for (let i = 0; i < nFeatures; i++) {
      const values = features.map(f => f[i]).sort((a, b) => a - b);
      const median = values[Math.floor(values.length / 2)];
      medians.push(median);
      
      const deviations = values.map(v => Math.abs(v - median));
      deviations.sort((a, b) => a - b);
      const mad = deviations[Math.floor(deviations.length / 2)] || 1;
      mads.push(mad);
    }

    // Score each point based on distance from median
    return features.map(point => {
      let totalDistance = 0;
      for (let i = 0; i < nFeatures; i++) {
        const normalizedDistance = Math.abs(point[i] - medians[i]) / (mads[i] || 1);
        totalDistance += normalizedDistance;
      }
      // Lower score = more anomalous (simplified)
      return -totalDistance / nFeatures;
    });
  }
}

export class IsolationForestEngine implements AnomalyEngine {
  private forests: Map<string, SimpleIsolationForest> = new Map();
  private windowSize: number;
  private thresholdPercentile: number;
  private recentPoints: Map<string, MetricPoint[]> = new Map();

  constructor(windowSize: number = 512, thresholdPercentile: number = 95) {
    this.windowSize = windowSize;
    this.thresholdPercentile = thresholdPercentile;
  }

  getType(): string {
    return 'isoforest';
  }

  async scoreBatch(deviceId: string, points: MetricPoint[]): Promise<AnomalyResult[]> {
    let devicePoints = this.recentPoints.get(deviceId) || [];

    // Add new points to window
    devicePoints.push(...points);
    
    // Keep only the most recent windowSize points
    if (devicePoints.length > this.windowSize) {
      devicePoints = devicePoints.slice(-this.windowSize);
    }
    this.recentPoints.set(deviceId, devicePoints);

    // Need at least 2 points to train
    if (devicePoints.length < 2) {
      return points.map((_, idx) => ({
        pointIndex: idx,
        score: 0,
        isAnomaly: false,
      }));
    }

    // Get or create forest for this device
    if (!this.forests.has(deviceId)) {
      this.forests.set(deviceId, new SimpleIsolationForest(0.1, 100, Math.min(256, devicePoints.length)));
    }

    const forest = this.forests.get(deviceId)!;

    // Prepare features
    const allFeatures = devicePoints.map(p => [
      p.temperature_c,
      p.vibration_g,
      p.humidity_pct,
      p.voltage_v,
    ]);

    const newFeatures = points.map(p => [
      p.temperature_c,
      p.vibration_g,
      p.humidity_pct,
      p.voltage_v,
    ]);

    // Score all points
    const allScores = forest.score(allFeatures);
    
    // Calculate threshold from percentile
    const sortedScores = [...allScores].sort((a, b) => a - b);
    const thresholdIndex = Math.floor(
      (sortedScores.length * (100 - this.thresholdPercentile)) / 100
    );
    const threshold = sortedScores[thresholdIndex] || 0;

    // Score only the new points
    const newScores = forest.score(newFeatures);

    // Return results
    return newScores.map((score, idx) => ({
      pointIndex: idx,
      score: Math.abs(score), // Use absolute value for display
      isAnomaly: score < threshold, // Lower scores indicate anomalies
    }));
  }
}

