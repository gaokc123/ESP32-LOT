import requests
import random
import time

# 这是我们在 Django 里定义的“进货口”地址
#url = "[http://127.0.0.1:8000/api/upload/](http://127.0.0.1:8000/api/upload/)"
url = "http://127.0.0.1:8000/api/upload/"
print("开始模拟设备发送数据... (按 Ctrl+C 停止)")

while True:
    # 1. 生成随机的温度和湿度
    # random.uniform(20, 30) 意思是产生一个 20 到 30 之间的随机小数
    temp = round(random.uniform(20.0, 35.0), 1)
    humi = round(random.uniform(40.0, 60.0), 1)
    
    # 2. 打包数据 (JSON 格式)
    data = {
        "temperature": temp,
        "humidity": humi
    }
    
    try:
        # 3. 发送给服务器 (这行代码等同于 ESP32 发请求)
        response = requests.post(url, json=data)
        
        # 4. 打印结果
        if response.status_code == 200:
            print(f"✅ 发送成功: 温度={temp}°C, 湿度={humi}% | 服务器回复: {response.json()['message']}")
        else:
            print(f"❌ 发送失败: {response.status_code}")
            
    except Exception as e:
        print(f"❌ 连接错误: {e}")
        print("请检查：1. Django 服务器开启了吗？ 2. 地址对不对？")
        break
        
    # 5. 休息 2 秒再发下一次
    time.sleep(2)