import React, { useState, useEffect, useRef } from 'react';
import {
    Chart as ChartJS,
    CategoryScale,
    LinearScale,
    BarElement,
    Title,
    Tooltip,
    Legend
} from 'chart.js';
import { Bar } from 'react-chartjs-2';
import { Activity, Cpu, Zap, Power, ShieldAlert, Sun, Eye, ThermometerSun } from 'lucide-react';

ChartJS.register(
    CategoryScale,
    LinearScale,
    BarElement,
    Title,
    Tooltip,
    Legend
);

interface SpectralData {
    F1?: number;
    F2?: number;
    F3?: number;
    F4?: number;
    F5?: number;
    F6?: number;
    F7?: number;
    F8?: number;
    UV: number;
    ALS: number;
    TSL_Lux: number;
    TSL_IR: number;
    TSL_Full: number;
}

const App: React.FC = () => {
    const [connected, setConnected] = useState(false);
    const [data, setData] = useState<SpectralData>({
        F1: 0, F2: 0, F3: 0, F4: 0, F5: 0, F6: 0, F7: 0, F8: 0,
        UV: 0, ALS: 0, TSL_Lux: 0, TSL_IR: 0, TSL_Full: 0
    });
    const [port, setPort] = useState<any>(null);
    const [error, setError] = useState<string | null>(null);
    const readerRef = useRef<any>(null);

    const connectSerial = async () => {
        try {
            if (!('serial' in navigator)) {
                setError("Web Serial API not supported. Use Chrome or Edge.");
                return;
            }
            const p = await (navigator as any).serial.requestPort();
            await p.open({ baudRate: 115200 });
            setPort(p);
            setConnected(true);
            setError(null);
            readLoop(p);
        } catch (err: any) {
            console.error(err);
            setError(err.message);
        }
    };

    const disconnectSerial = async () => {
        if (readerRef.current) {
            await readerRef.current.cancel();
        }
        if (port) {
            await port.close();
        }
        setPort(null);
        setConnected(false);
    };

    const readLoop = async (p: any) => {
        const textDecoder = new TextDecoderStream();
        const readableStreamClosed = p.readable.pipeTo(textDecoder.writable);
        const reader = textDecoder.readable.getReader();
        readerRef.current = reader;
        let buffer = "";

        try {
            while (true) {
                const { value, done } = await reader.read();
                if (done) break;
                buffer += value;
                const lines = buffer.split("\n");
                buffer = lines.pop() || "";
                for (const line of lines) {
                    try {
                        const cleanLine = line.trim();
                        if (cleanLine.startsWith("{") && cleanLine.endsWith("}")) {
                            const json = JSON.parse(cleanLine);
                            setData(prev => ({ ...prev, ...json }));
                        }
                    } catch (e) { }
                }
            }
        } catch (err) {
            console.error(err);
        } finally {
            reader.releaseLock();
        }
    };

    const spectralDataValues = [data.F1 || 0, data.F2 || 0, data.F3 || 0, data.F4 || 0, data.F5 || 0, data.F6 || 0, data.F7 || 0, data.F8 || 0];

    const chartData = {
        labels: ['415nm', '445nm', '480nm', '515nm', '555nm', '590nm', '630nm', '680nm'],
        datasets: [
            {
                label: 'Intensity',
                data: spectralDataValues,
                backgroundColor: ['#4B0082', '#0000FF', '#00FF00', '#FFFF00', '#FFA500', '#FF0000', '#8B0000', '#4B0000'],
                borderRadius: 8,
            },
        ],
    };

    const options = {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { display: false },
        },
        scales: {
            y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#909090' } },
            x: { grid: { display: false }, ticks: { color: '#909090' } }
        }
    };

    return (
        <div className="dashboard">
            <header className="header">
                <div className="title-group">
                    <h1>Multi-Sensor Light Lab</h1>
                    <p>Spectral, UV, and Luminosity Monitoring</p>
                </div>
                <div className="controls">
                    <button className={`btn ${connected ? '' : 'primary'}`} onClick={connected ? disconnectSerial : connectSerial}>
                        <Power size={18} /> {connected ? 'Disconnect' : 'Connect Device'}
                    </button>
                </div>
            </header>

            <main className="main-content">
                <div className="chart-card">
                    <div style={{ flex: 1 }}><Bar data={chartData} options={options} /></div>
                </div>

                <aside className="sidebar">
                    <div className="status-card">
                        <div className="sensor-stat">
                            <Sun className="text-yellow" size={24} />
                            <div>
                                <span className="label">UV Index (LTR390)</span>
                                <span className="val">{data.UV}</span>
                            </div>
                        </div>
                        <div className="sensor-stat">
                            <Eye className="text-cyan" size={24} />
                            <div>
                                <span className="label">Luminosity (TSL2591)</span>
                                <span className="val">{data.TSL_Lux.toFixed(2)} Lux</span>
                            </div>
                        </div>
                    </div>

                    <div className="spectral-grid">
                        <div className="channel-box extra">
                            <span className="val">{data.ALS}</span>
                            <span className="label">LTR ALS</span>
                        </div>
                        <div className="channel-box extra">
                            <span className="val">{data.TSL_IR}</span>
                            <span className="label">TSL IR</span>
                        </div>
                    </div>

                    <div className="spectral-grid">
                        {['F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8'].map((key) => (
                            <div key={key} className={`channel-box ${(key).toLowerCase()}`}>
                                <span className="val">{(data as any)[key]}</span>
                                <span className="label">{key}</span>
                            </div>
                        ))}
                    </div>
                </aside>
            </main>
        </div>
    );
};

export default App;
