import os
import numpy as np
from scipy.io import wavfile
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.shortcuts import render
from .models import SensorData

# ==========================================
# 1. 接收 ESP32 上传的音频并存入数据库
# ==========================================
@csrf_exempt 
def receive_data(request):
    if request.method == 'POST':
        try:
            if request.FILES.get('audio_file'):
                audio = request.FILES.get('audio_file')
                original_name = audio.name  # 这里应该是 "raw.wav" 或 "processed.wav"
                
                # 在存入数据库前，手动给文件名打个标签，防止 Django 自动重命名干扰
                # SensorData.objects.create 会自动保存文件
                instance = SensorData.objects.create(audio_file=audio)
                
                # 打印一下，看看后台收到的到底是什么名字
                print(f"收到文件: {original_name}, 已保存 ID: {instance.id}")
                
                return JsonResponse({"status": "success", "id": instance.id})
            else:
                return JsonResponse({"status": "error", "message": "No audio file found"})
        except Exception as e:
            print(f"保存失败: {str(e)}")
            return JsonResponse({"status": "error", "message": str(e)})
    return JsonResponse({"status": "error", "message": "Only POST allowed"})

# ==========================================
# 2. 获取历史记录列表 (供前端下拉菜单使用)
# ==========================================
def get_history(request):
    # 获取最近 40 条记录
    raw_data = SensorData.objects.all().order_by('-timestamp')[:40]
    results = []
    
    for item in raw_data:
        if item.audio_file:
            name = item.audio_file.name.lower()
            # 根据文件名区分是降噪前还是降噪后
            file_type = "raw" if "raw" in name else "processed"
            
            results.append({
                "id": item.id,
                "type": file_type,
                "time": item.timestamp.strftime("%Y-%m-%d %H:%M:%S"),
                "url": item.audio_file.url  # 用于前端播放和下载的相对路径
            })
    
    return JsonResponse({"data": results})

# ==========================================
# 3. 根据指定的 ID，实时计算该音频的 FFT 频谱
# ==========================================
def get_fft_by_id(request):
    record_id = request.GET.get('id')
    if not record_id:
        return JsonResponse({"error": "Missing ID parameter"})
    
    try:
        item = SensorData.objects.get(id=record_id)
        file_path = item.audio_file.path
        # --- 核心修改：检查文件大小 ---
        if os.path.getsize(file_path) < 100: # 有效 WAV 头至少 44 字节
            return JsonResponse({"status": "error", "error": "音频文件损坏或上传不完整（文件太小）"})
            
        # 读取并捕捉 scipy 可能抛出的格式错误
        try:
            sample_rate, data = wavfile.read(file_path)
        except Exception as wav_err:
            return JsonResponse({"status": "error", "error": f"WAV格式解析失败: {str(wav_err)}"})
        # 读取 WAV 文件
        sample_rate, data = wavfile.read(file_path)
        # 归一化
        data = data / 32768.0 
        
        # 傅里叶变换
        n = len(data)
        fft_mag = np.abs(np.fft.rfft(data)) / n
        fft_freq = np.fft.rfftfreq(n, d=1.0/sample_rate)
        
        # 下采样，防止浏览器卡顿
        downsample_step = max(1, len(fft_freq) // 1000)
        
        return JsonResponse({
            "status": "success",
            "fft_freq": np.round(fft_freq[::downsample_step], 1).tolist(),
            "fft_mag": np.round(fft_mag[::downsample_step], 5).tolist()
        })
    except Exception as e:
        return JsonResponse({"error": str(e)})

# ==========================================
# 4. 返回前端网页
# ==========================================
def index_page(request):
    return render(request, 'index.html')

# ---------------------------------------------------------
# 这里的全局变量用于兼容你之前可能存在的旧版前端轮询逻辑
# 如果你已经完全使用了“下拉菜单选择历史”的逻辑，这个可以只留空
# ---------------------------------------------------------
LATEST_AUDIO_DATA = {
    "raw": {"fft_freq": [], "fft_mag": []},
    "processed": {"fft_freq": [], "fft_mag": []}
}

def get_audio_data(request):
    """前端轮询拉取最新 FFT 数据的接口"""
    return JsonResponse(LATEST_AUDIO_DATA)