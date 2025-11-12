import type { Metadata } from 'next'
import './globals.css'

export const metadata: Metadata = {
  title: 'IoT Anomaly Detection Dashboard',
  description: 'Real-time monitoring and anomaly detection for IoT devices',
}

export default function RootLayout({
  children,
}: {
  children: React.ReactNode
}) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  )
}

