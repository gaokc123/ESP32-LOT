import { useState, useEffect } from 'react'
import axios from 'axios'
import { 
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer 
} from 'recharts'
import './App.css'

function App() {
  const [data, setData] = useState([])
  const [currentTemp, setCurrentTemp] = useState(0)
  const [currentHumi, setCurrentHumi] = useState(0)

  // 1. 获取数据的逻辑 (保持不变)
  const fetchData = () => {
    // 纯净的 API 地址
    axios.get('http://127.0.0.1:8000/api/history/')
      .then(response => {
        const historyData = response.data.data
        if (historyData.length > 0) {
          setData([...historyData].reverse())
          setCurrentTemp(historyData[0].temp)
          setCurrentHumi(historyData[0].humi)
        }
      })
      .catch(error => {
        console.error("数据获取失败:", error)
      })
  }

  // 2. 自动刷新逻辑 (保持不变)
  useEffect(() => {
    fetchData()
    const interval = setInterval(fetchData, 2000)
    return () => clearInterval(interval)
  }, [])

  // 3. 界面渲染 (全新升级)
  return (
    <div style={styles.pageContainer}>
      <div style={styles.dashboard}>
        {/* 标题栏 */}
        <header style={styles.header}>
          <h1 style={styles.title}>🌡️ 智能环境监测系统</h1>
          <p style={styles.subtitle}>实时物联网数据监控大屏</p>
        </header>
        
        {/* 核心数据卡片区 */}
        <div style={styles.cardsContainer}>
          {/* 温度卡片 */}
          <div style={{...styles.card, borderTop: '4px solid #ff7875'}}>
            <h3 style={styles.cardTitle}>实时温度</h3>
            <div style={styles.dataRow}>
              <span style={{...styles.bigNumber, color: '#d4380d'}}>{currentTemp}</span>
              <span style={styles.unit}>°C</span>
            </div>
            <p style={styles.statusText}>
              {currentTemp > 30 ? '⚠️ 温度过高' : '✅ 温度正常'}
            </p>
          </div>

          {/* 湿度卡片 */}
          <div style={{...styles.card, borderTop: '4px solid #95de64'}}>
            <h3 style={styles.cardTitle}>实时湿度</h3>
            <div style={styles.dataRow}>
              <span style={{...styles.bigNumber, color: '#389e0d'}}>{currentHumi}</span>
              <span style={styles.unit}>%</span>
            </div>
            <p style={styles.statusText}>
              {currentHumi > 80 ? '💧 湿度过大' : '✅ 湿度适宜'}
            </p>
          </div>
        </div>

        {/* 图表区 */}
        <div style={styles.chartSection}>
          <h3 style={styles.chartTitle}>📊 24小时温湿度变化趋势</h3>
          <div style={styles.chartWrapper}>
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={data} margin={{ top: 5, right: 20, bottom: 5, left: 0 }}>
                <CartesianGrid stroke="#f0f0f0" strokeDasharray="3 3" />
                <XAxis dataKey="time" tick={{fontSize: 12, fill: '#666'}} />
                <YAxis tick={{fontSize: 12, fill: '#666'}} />
                <Tooltip 
                  contentStyle={{borderRadius: '8px', border: 'none', boxShadow: '0 4px 12px rgba(0,0,0,0.1)'}}
                />
                <Legend />
                {/* 温度线：平滑曲线，橙红色 */}
                <Line 
                  type="monotone" 
                  dataKey="temp" 
                  stroke="#ff7875" 
                  strokeWidth={3} 
                  dot={{r: 4}} 
                  activeDot={{r: 8}}
                  name="温度"
                />
                {/* 湿度线：平滑曲线，翠绿色 */}
                <Line 
                  type="monotone" 
                  dataKey="humi" 
                  stroke="#52c41a" 
                  strokeWidth={3} 
                  dot={{r: 4}} 
                  activeDot={{r: 8}} 
                  name="湿度"
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>
      </div>
    </div>
  )
}

// ----------------------------------------------------------------------
// 样式表 (CSS-in-JS 写法，方便你直接复制)
// ----------------------------------------------------------------------
const styles = {
  pageContainer: {
    minHeight: '100vh',
    backgroundColor: '#f0f2f5', // 浅灰背景，不刺眼
    display: 'flex',
    justifyContent: 'center',
    padding: '40px 20px',
    boxSizing: 'border-box',
  },
  dashboard: {
    width: '100%',
    maxWidth: '1000px', // 限制最大宽度，防止在大屏上太散
    display: 'flex',
    flexDirection: 'column',
    gap: '24px',
  },
  header: {
    textAlign: 'center',
    marginBottom: '10px',
  },
  title: {
    margin: 0,
    fontSize: '28px',
    color: '#1f1f1f',
    letterSpacing: '1px',
  },
  subtitle: {
    margin: '8px 0 0',
    color: '#8c8c8c',
    fontSize: '14px',
  },
  cardsContainer: {
    display: 'flex',
    gap: '24px',
    justifyContent: 'space-between',
    flexWrap: 'wrap', // 手机端自动换行
  },
  card: {
    flex: 1,
    minWidth: '280px', // 防止卡片变得太窄
    backgroundColor: '#fff',
    borderRadius: '12px',
    padding: '24px',
    boxShadow: '0 2px 8px rgba(0,0,0,0.06)', // 细腻的阴影
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
  },
  cardTitle: {
    margin: 0,
    fontSize: '16px',
    color: '#8c8c8c',
    fontWeight: '500',
  },
  dataRow: {
    display: 'flex',
    alignItems: 'baseline',
    margin: '16px 0',
  },
  bigNumber: {
    fontSize: '48px',
    fontWeight: 'bold',
    lineHeight: 1,
  },
  unit: {
    fontSize: '20px',
    color: '#8c8c8c',
    marginLeft: '8px',
  },
  statusText: {
    margin: 0,
    padding: '4px 12px',
    backgroundColor: '#f5f5f5',
    borderRadius: '20px',
    fontSize: '13px',
    color: '#595959',
  },
  chartSection: {
    backgroundColor: '#fff',
    borderRadius: '12px',
    padding: '24px',
    boxShadow: '0 2px 8px rgba(0,0,0,0.06)',
  },
  chartTitle: {
    margin: '0 0 20px 0',
    fontSize: '18px',
    color: '#262626',
  },
  chartWrapper: {
    width: '100%',
    height: '400px',
  }
}

export default App