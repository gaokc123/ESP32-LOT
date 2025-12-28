import { useState, useEffect } from 'react'
import axios from 'axios'
import './App.css'

function App() {
  const [data, setData] = useState([])

  // 获取数据
  const fetchData = () => {
    axios.get('http://127.0.0.1:8000/api/history/')
      .then(response => {
        setData(response.data.data)
      })
      .catch(error => {
        console.error("数据获取失败:", error)
      })
  }

  // 自动刷新
  useEffect(() => {
    fetchData()
    const interval = setInterval(fetchData, 2000)
    return () => clearInterval(interval)
  }, [])

  return (
    <div style={styles.pageContainer}>
      <div style={styles.dashboard}>
        {/* 标题栏 */}
        <header style={styles.header}>
          <h1 style={styles.title}>🎙️ 智能语音记录仪</h1>
          <p style={styles.subtitle}>ESP32 实时录音上传系统</p>
        </header>
        
        {/* 音频列表区 */}
        <div style={styles.listSection}>
            <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px'}}>
                <h3 style={styles.sectionTitle}>最近录音记录 ({data.length})</h3>
                <button onClick={fetchData} style={styles.refreshButton}>🔄 刷新列表</button>
            </div>

            <div style={styles.tableContainer}>
                {data.length === 0 ? (
                    <div style={{textAlign: 'center', padding: '40px', color: '#999'}}>
                        暂无录音记录，请按下 ESP32 录音键...
                    </div>
                ) : (
                    <table style={{width: '100%', borderCollapse: 'collapse'}}>
                        <thead>
                            <tr style={{textAlign: 'left', borderBottom: '2px solid #f0f0f0'}}>
                                <th style={styles.th}>时间</th>
                                <th style={styles.th}>播放录音</th>
                                <th style={styles.th}>操作</th>
                            </tr>
                        </thead>
                        <tbody>
                            {data.map((item) => (
                                <tr key={item.id} style={{borderBottom: '1px solid #f9f9f9'}}>
                                    <td style={styles.tdTime}>{item.time}</td>
                                    <td style={styles.td}>
                                        {item.audio_url && (
                                            <audio controls style={{height: '36px', width: '260px'}}>
                                                <source src={item.audio_url} type="audio/wav" />
                                                您的浏览器不支持播放
                                            </audio>
                                        )}
                                    </td>
                                    <td style={styles.td}>
                                        {item.audio_url && (
                                            <a 
                                                href={item.audio_url} 
                                                download={`record_${item.id}.wav`}
                                                target="_blank" 
                                                rel="noopener noreferrer"
                                                style={styles.downloadButton}
                                            >
                                                ⬇️ 下载
                                            </a>
                                        )}
                                    </td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                )}
            </div>
        </div>
      </div>
    </div>
  )
}

// ----------------------------------------------------------------------
// 样式表
// ----------------------------------------------------------------------
const styles = {
  pageContainer: {
    minHeight: '100vh',
    backgroundColor: '#f5f7fa',
    display: 'flex',
    justifyContent: 'center',
    padding: '40px 20px',
    boxSizing: 'border-box',
    fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif',
  },
  dashboard: {
    width: '100%',
    maxWidth: '900px',
    display: 'flex',
    flexDirection: 'column',
    gap: '20px',
  },
  header: {
    textAlign: 'center',
    marginBottom: '20px',
  },
  title: {
    margin: 0,
    fontSize: '32px',
    color: '#333',
    fontWeight: '700',
  },
  subtitle: {
    margin: '8px 0 0',
    color: '#666',
    fontSize: '16px',
  },
  listSection: {
    backgroundColor: '#fff',
    borderRadius: '16px',
    padding: '30px',
    boxShadow: '0 4px 20px rgba(0,0,0,0.05)',
  },
  sectionTitle: {
    margin: 0,
    fontSize: '20px',
    color: '#333',
  },
  refreshButton: {
    padding: '8px 16px',
    backgroundColor: '#eff2f5',
    border: 'none',
    borderRadius: '8px',
    cursor: 'pointer',
    color: '#555',
    fontWeight: '600',
    transition: '0.2s',
  },
  tableContainer: {
    overflowX: 'auto',
  },
  th: {
    padding: '15px',
    color: '#888',
    fontSize: '14px',
    fontWeight: '600',
  },
  td: {
    padding: '15px',
    verticalAlign: 'middle',
  },
  tdTime: {
    padding: '15px',
    color: '#333',
    fontSize: '15px',
    fontFamily: 'monospace',
    verticalAlign: 'middle',
  },
  downloadButton: {
    textDecoration: 'none', 
    color: '#007aff', 
    backgroundColor: 'rgba(0,122,255,0.1)', 
    padding: '8px 16px', 
    borderRadius: '6px',
    fontSize: '13px',
    fontWeight: '500',
    display: 'inline-block',
  }
}

export default App