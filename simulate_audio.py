import requests
import random
import time
import io

# 后端接口地址
url = "http://127.0.0.1:8000/api/upload/"

print("开始模拟音频上传... (按 Ctrl+C 停止)")

while True:
    try:
        # 1. 生成随机温湿度
        temp = round(random.uniform(20.0, 35.0), 1)
        humi = round(random.uniform(40.0, 60.0), 1)

        # 2. 创建一个假的音频文件 (内存里的虚拟文件)
        # 实际上这只是一个文本文件，但我们假装它是 .wav
        # 你可以真的放一个 test.wav 在同级目录，然后用 open('test.wav', 'rb')
        fake_audio_content = b"RIFF....WAVEfmt ..... (this is fake audio data from python script)"
        audio_file = io.BytesIO(fake_audio_content)
        audio_file.name = 'test_recording.wav'

        # 3. 准备表单数据 (数值)
        payload = {
            "temperature": temp,
            "humidity": humi
        }

        # 4. 准备文件数据
        # 格式: '字段名': (文件名, 文件内容, 文件类型)
        # 注意：这里的 key 'audio_file' 必须和 Django views.py 里 request.FILES.get('audio_file') 一样
        files = {
            "audio_file": ("recording.wav", audio_file, "audio/wav")
        }

        # 5. 发送请求
        # 注意：这里用 data=... 和 files=...，不再用 json=...
        response = requests.post(url, data=payload, files=files)

        if response.status_code == 200:
            print(f"✅ 上传成功: Temp={temp}°C | 有音频文件! | 服务器回复: {response.json()['message']}")
        else:
            print(f"❌ 失败: {response.text}")

    except Exception as e:
        print(f"❌ 错误: {e}")
        print("请检查 Django 是否运行？地址是否正确？")

    # 每5秒发一次
    time.sleep(5)