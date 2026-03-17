import os
import numpy as np
from scipy.io import wavfile
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.shortcuts import render
from .models import SensorData

# ---------------------------------------------------------
# 全局变量：用于暂存最新一次上传的 FFT 数据供前端图表实时拉取
# ---------------------------------------------------------
LATEST_AUDIO_DATA = {
    "raw": {"fft_freq": [], "fft_mag": []},
    "processed": {"fft_freq": [], "fft_mag": []}
}

# ---------------------------------------------------------
# 接口 1: 接收音频文件，存入数据库，并进行 FFT 运算
# (对应 ESP32 代码中的 /api/upload/ 接口)
# ---------------------------------------------------------
@csrf_exempt 
def receive_data(request):
    if request.method == 'POST':
        try:
            # 检查是否有文件上传 (ESP32 代码里定义的 name="audio_file")
            if request.FILES.get('audio_file'):
                audio = request.FILES.get('audio_file')
                
                # 1. 直接存入数据库，Django 会自动处理文件保存到 media 文件夹
                item = SensorData.objects.create(audio_file=audio)
                
                # 2. 提取文件类型：判断是降噪前(raw)还是降噪后(processed)
                filename = audio.name.lower()
                if "raw" in filename:
                    file_type = "raw"
                else:
                    file_type = "processed"
                
                # 3. 读取刚刚保存到本地硬盘的 WAV 文件
                file_path = item.audio_file.path
                sample_rate, data = wavfile.read(file_path)
                
                # 将 16bit PCM 数据归一化到 [-1.0, 1.0]
                data = data / 32768.0 
                
                # 4. 执行 FFT (快速傅里叶变换)
                n = len(data)
                fft_mag = np.abs(np.fft.rfft(data)) / n
                fft_freq = np.fft.rfftfreq(n, d=1.0/sample_rate)
                
                # 数据下采样：防止几万个点把前端浏览器卡死，等比例抽取约 1000 个点
                downsample_step = max(1, len(fft_freq) // 1000)
                
                # 5. 更新全局字典，供前端 ECharts 页面读取
                LATEST_AUDIO_DATA[file_type] = {
                    "fft_freq": np.round(fft_freq[::downsample_step], 1).tolist(),
                    "fft_mag": np.round(fft_mag[::downsample_step], 5).tolist()
                }
                
                print(f"成功保存并分析了音频: {filename}")
                return JsonResponse({"status": "success", "message": f"{file_type} saved and processed!"})
            else:
                return JsonResponse({"status": "error", "message": "No audio file found"})
            
        except Exception as e:
            print(f"处理失败: {e}")
            return JsonResponse({"status": "error", "message": str(e)})
            
    return JsonResponse({"status": "error", "message": "Only POST allowed"})

# ---------------------------------------------------------
# 接口 2: 获取音频历史列表 (保留你原来的功能)
# ---------------------------------------------------------
def get_history(request):
    # 取最近 20 条记录，按时间倒序
    raw_data = SensorData.objects.all().order_by('-timestamp')[:20]
    result_list = []
    
    for item in raw_data:
        audio_url = ""
        if item.audio_file:
            # 构建完整的文件访问链接
            audio_url = request.build_absolute_uri(item.audio_file.url)
            
        result_list.append({
            "id": item.id,
            "time": item.timestamp.strftime("%Y-%m-%d %H:%M:%S"),
            "audio_url": audio_url
        })
    
    return JsonResponse({"data": result_list})

# ---------------------------------------------------------
# 接口 3: 前端图表轮询获取最新 FFT 数据的接口 (新增)
# ---------------------------------------------------------
def get_audio_data(request):
    return JsonResponse(LATEST_AUDIO_DATA)

# ---------------------------------------------------------
# 接口 4: 返回包含 ECharts 的可视化网页 (新增)
# ---------------------------------------------------------
def index_page(request):
    return render(request, 'index.html')