'use client';

import { useEffect, useState } from 'react';
import { useParams } from 'next/navigation';
import Link from 'next/link';
import { fetchDevice, fetchMetrics, Device, Metric } from '@/lib/api';
import { getSocket } from '@/lib/socket';
import LiveStatus from '@/components/LiveStatus';
import MetricChart from '@/components/MetricChart';
import { subMinutes, subHours, subDays } from 'date-fns';

type TimeRange = '15m' | '1h' | '24h' | '7d';

export default function DeviceDetailPage() {
  const params = useParams();
  const deviceId = params.id as string;

  const [device, setDevice] = useState<Device | null>(null);
  const [metrics, setMetrics] = useState<Metric[]>([]);
  const [timeRange, setTimeRange] = useState<TimeRange>('1h');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (!deviceId) return;

    loadDevice();
    loadMetrics();

    const socket = getSocket();
    socket.emit('subscribe:device', deviceId);

    const onMetricNew = (data: { deviceId: string; metric: Metric }) => {
      if (data.deviceId === deviceId) {
        setMetrics((prev) => [data.metric, ...prev].slice(0, 1000));
      }
    };

    socket.on('metric:new', onMetricNew);

    return () => {
      socket.emit('unsubscribe:device', deviceId);
      socket.off('metric:new', onMetricNew);
    };
  }, [deviceId, timeRange]);

  const loadDevice = async () => {
    try {
      const deviceData = await fetchDevice(deviceId);
      setDevice(deviceData);
    } catch (error) {
      console.error('Failed to load device:', error);
    }
  };

  const loadMetrics = async () => {
    try {
      let from: Date;
      switch (timeRange) {
        case '15m':
          from = subMinutes(new Date(), 15);
          break;
        case '1h':
          from = subHours(new Date(), 1);
          break;
        case '24h':
          from = subHours(new Date(), 24);
          break;
        case '7d':
          from = subDays(new Date(), 7);
          break;
      }

      const metricsData = await fetchMetrics({
        deviceId,
        from: from.toISOString(),
        limit: 1000,
      });
      setMetrics(metricsData);
    } catch (error) {
      console.error('Failed to load metrics:', error);
    } finally {
      setLoading(false);
    }
  };

  if (loading) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-xl">Loading...</div>
      </div>
    );
  }

  if (!device) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-xl text-red-600">Device not found</div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50 p-8">
      <div className="max-w-7xl mx-auto">
        <div className="flex justify-between items-center mb-8">
          <div>
            <Link href="/devices" className="text-blue-600 hover:text-blue-800 mb-2 inline-block">
              ← Back to Devices
            </Link>
            <h1 className="text-3xl font-bold text-gray-900">{device.name}</h1>
            <p className="text-gray-600 mt-1">
              {device.location || 'No location'} • {device.id}
            </p>
          </div>
          <LiveStatus />
        </div>

        {/* Time Range Selector */}
        <div className="mb-6 flex gap-2">
          {(['15m', '1h', '24h', '7d'] as TimeRange[]).map((range) => (
            <button
              key={range}
              onClick={() => {
                setTimeRange(range);
                setLoading(true);
                loadMetrics();
              }}
              className={`px-4 py-2 rounded ${
                timeRange === range
                  ? 'bg-blue-600 text-white'
                  : 'bg-white text-gray-700 hover:bg-gray-100'
              }`}
            >
              {range}
            </button>
          ))}
        </div>

        {/* Charts */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4">Temperature</h2>
            <MetricChart
              metrics={metrics}
              metricKey="temperature_c"
              label="Temperature"
              unit="°C"
              color="#ef4444"
            />
          </div>

          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4">Vibration</h2>
            <MetricChart
              metrics={metrics}
              metricKey="vibration_g"
              label="Vibration"
              unit="g"
              color="#f59e0b"
            />
          </div>

          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4">Humidity</h2>
            <MetricChart
              metrics={metrics}
              metricKey="humidity_pct"
              label="Humidity"
              unit="%"
              color="#3b82f6"
            />
          </div>

          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4">Voltage</h2>
            <MetricChart
              metrics={metrics}
              metricKey="voltage_v"
              label="Voltage"
              unit="V"
              color="#10b981"
            />
          </div>
        </div>
      </div>
    </div>
  );
}

