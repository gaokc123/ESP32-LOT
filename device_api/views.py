from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import SensorData

# ---------------------------------------------------------
# 接口 1: 接收音频文件
# ---------------------------------------------------------
@csrf_exempt 
def receive_data(request):
    if request.method == 'POST':
        try:
            # 检查是否有文件上传
            # 'audio_file' 是前端/ESP32 上传时的 key
            if request.FILES.get('audio_file'):
                audio = request.FILES.get('audio_file')
                
                # 直接存入数据库，Django 会自动处理文件保存
                SensorData.objects.create(audio_file=audio)
                
                return JsonResponse({"status": "success", "message": "Audio saved!"})
            else:
                return JsonResponse({"status": "error", "message": "No audio file found"})
            
        except Exception as e:
            return JsonResponse({"status": "error", "message": str(e)})
            
    return JsonResponse({"status": "error", "message": "Only POST allowed"})

# ---------------------------------------------------------
# 接口 2: 获取音频历史列表
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
            # 格式化时间为 "YYYY-MM-DD HH:MM:SS"
            "time": item.timestamp.strftime("%Y-%m-%d %H:%M:%S"),
            "audio_url": audio_url
        })
    
    return JsonResponse({"data": result_list})