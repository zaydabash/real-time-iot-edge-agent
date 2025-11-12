# Real-Time C++ Edge Agent for IoT Metrics

A production-ready, multi-service system for collecting IoT device metrics, performing real-time anomaly detection, and visualizing data through a modern web dashboard.

## Architecture

```
┌─────────────┐      HTTP POST       ┌──────────────┐
│  C++ Agent  │─────────────────────>│   Backend    │
│ (Simulator) │                      │  (Express)   │
└─────────────┘                      └──────┬───────┘
                                             │
                                             │ Socket.IO
                                             │
                                    ┌────────▼────────┐
                                    │   PostgreSQL    │
                                    │   (Prisma ORM)  │
                                    └────────┬────────┘
                                             │
                                             │ REST API
                                             │
                                    ┌────────▼────────┐
                                    │   Dashboard    │
                                    │   (Next.js)    │
                                    └────────────────┘
```

### Data Flow

1. **C++ Agent** simulates IoT devices, generating metrics (temperature, vibration, humidity, voltage) with configurable intervals
2. **Backend** receives metrics via REST API, stores in PostgreSQL, runs anomaly detection (Isolation Forest or Z-Score)
3. **Anomalies** are detected in real-time and broadcast via Socket.IO
4. **Dashboard** visualizes metrics and anomalies in real-time using React and Recharts

## Quick Start (1 Minute)

### Prerequisites

- Docker & Docker Compose
- Node.js 20+ (for local development)
- CMake 3.10+ and libcurl (for C++ agent)
- Make

### Start Services

```bash
# Clone and navigate to project
cd edge-iot-anomaly-agent

# Start backend, database, and dashboard
cd infra && docker compose up --build
```

This will:
- Start PostgreSQL on port 5432
- Run Prisma migrations automatically
- Seed demo devices
- Start backend API on http://localhost:8080
- Start dashboard on http://localhost:3000

### Run Agent

In a new terminal:

```bash
make run-agent
```

The agent will start streaming metrics. Open http://localhost:3000 to see the dashboard update in real-time.

## Project Structure

```
edge-iot-anomaly-agent/
├── agent-cpp/          # C++17 IoT simulator
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── http_client.hpp/cpp
│   │   └── config.hpp/cpp
│   └── config/
│       └── agent.json
├── backend/            # Node.js + Express + Prisma
│   ├── src/
│   │   ├── index.ts
│   │   ├── routes/
│   │   ├── realtime.ts
│   │   └── anomaly/
│   ├── prisma/
│   │   └── schema.prisma
│   ├── scripts/
│   └── Dockerfile
├── dashboard/          # Next.js 14+ App Router
│   ├── app/
│   ├── components/
│   └── Dockerfile
├── infra/              # Docker Compose
│   └── docker-compose.yml
├── Makefile
├── README.md
└── .env.example
```

## Configuration

### Environment Variables

Copy `.env.example` to `.env` and customize:

```bash
# Database
DATABASE_URL=postgresql://postgres:postgres@localhost:5432/iot

# Backend
BACKEND_PORT=8080
ANOMALY_ENGINE=isoforest  # or 'zscore'
ANOMALY_WINDOW_SIZE=512
ALLOW_AUTO_DEVICE=true

# Dashboard
NEXT_PUBLIC_BACKEND_URL=http://localhost:8080
```

### Agent Configuration

Edit `agent-cpp/config/agent.json`:

```json
{
  "device_id": "sim-device-001",
  "api_base_url": "http://localhost:8080",
  "interval_ms": 1000,
  "jitter_ms": 100,
  "anomaly_probability": 0.05,
  "metrics": {
    "temperature": true,
    "vibration": true,
    "humidity": true,
    "voltage": true
  }
}
```

Or override via CLI:

```bash
./agent --device_id=my-device --api_base_url=http://localhost:8080 --interval_ms=2000
```

## API Documentation

### Ingest Metrics

```bash
POST /api/ingest
Content-Type: application/json

{
  "deviceId": "sim-device-001",
  "metrics": [
    {
      "ts": "2024-01-01T12:00:00Z",
      "temperature_c": 22.5,
      "vibration_g": 0.02,
      "humidity_pct": 45.0,
      "voltage_v": 4.9
    }
  ]
}
```

### List Devices

```bash
GET /api/devices
```

### Query Metrics

```bash
GET /api/metrics?deviceId=sim-device-001&from=2024-01-01T00:00:00Z&to=2024-01-01T23:59:59Z&limit=100
```

### Query Anomalies

```bash
GET /api/anomalies?deviceId=sim-device-001&from=2024-01-01T00:00:00Z&type=isoforest
```

### Health Check

```bash
GET /api/health
```

## Anomaly Detection

The system supports two anomaly detection engines:

### Isolation Forest (`isoforest`)

- Simplified isolation forest implementation using median absolute deviation (MAD)
- Trains on a sliding window of recent metrics (default: 512 points)
- Features: temperature_c, vibration_g, humidity_pct, voltage_v
- Flags anomalies based on score percentile threshold
- Note: For production, consider using a full isolation forest library

### Z-Score (`zscore`)

- Rolling window mean and standard deviation per metric per device
- Default window: 200 points
- Flags anomalies when |z-score| > 3

### Switching Engines

Set environment variable:

```bash
ANOMALY_ENGINE=zscore  # or 'isoforest'
```

The backend will automatically use the selected engine without code changes.

## Real-Time Updates

The dashboard connects to Socket.IO for real-time updates:

- `metric:new` - New metric received
- `anomaly:new` - Anomaly detected
- `device:update` - Device status changed

## Development

### Backend

```bash
cd backend
npm install
npm run dev          # Start with hot reload
npm test             # Run tests
npm run lint         # Lint code
npx prisma studio    # Open Prisma Studio
```

### Dashboard

```bash
cd dashboard
npm install
npm run dev          # Start dev server
npm test             # Run tests
npm run lint         # Lint code
```

### Agent

```bash
cd agent-cpp
mkdir -p build && cd build
cmake ..
make
./agent
```

## Testing

### Backend Tests

```bash
cd backend
npm test
```

Tests include:
- Route handlers (ingest, devices, metrics, anomalies)
- Anomaly detection engines
- Socket.IO event emission

### Dashboard Tests

```bash
cd dashboard
npm test
```

Tests include:
- Component rendering
- API integration
- Socket.IO client

### Integration Test

```bash
# Start services
make dev

# In another terminal, run agent
make run-agent

# Verify metrics appear in dashboard at http://localhost:3000
```

## Troubleshooting

### Database Connection Issues

```bash
# Check if PostgreSQL is running
docker ps | grep postgres

# Check logs
cd infra && docker compose logs db

# Reset database
make down
make db-migrate
make db-seed
```

### Agent Not Connecting

- Verify backend is running: `curl http://localhost:8080/api/health`
- Check agent config: `cat agent-cpp/config/agent.json`
- Verify network: agent should reach `http://localhost:8080`

### No Anomalies Detected

- Check anomaly engine: `echo $ANOMALY_ENGINE`
- Increase anomaly probability in agent config
- Verify metrics are being ingested: `curl http://localhost:8080/api/metrics?limit=10`

### Dashboard Not Updating

- Check Socket.IO connection in browser console
- Verify `NEXT_PUBLIC_BACKEND_URL` matches backend URL
- Check backend logs for Socket.IO errors

## CI/CD

GitHub Actions workflow (`.github/workflows/ci.yml`) runs on push:

- Builds backend, dashboard, and agent
- Runs tests
- Validates Prisma schema
- Lints code

## Production Deployment

### Backend

1. Set production environment variables
2. Run migrations: `npx prisma migrate deploy`
3. Build: `npm run build`
4. Start: `npm start`

### Dashboard

1. Set `NEXT_PUBLIC_BACKEND_URL` to production backend URL
2. Build: `npm run build`
3. Start: `npm start`

### Agent

1. Build for target platform (Raspberry Pi ARM)
2. Configure `agent.json` with production backend URL
3. Run as systemd service or container

## License

MIT

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes
4. Add tests
5. Submit a pull request

