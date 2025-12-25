from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
import json
from .models import SensorData

# ---------------------------------------------------------
# 接口 1: 接收数据 (专门给 ESP32 用)
# ---------------------------------------------------------
# @csrf_exempt 的意思是：允许“陌生人”发请求。
# 因为 ESP32 只是个芯片，没有浏览器的安全证书，不加这个它发不进来。
@csrf_exempt 
def receive_data(request):
    if request.method == 'POST':
        try:
            # 1. 拆开 ESP32 发来的包裹 (JSON数据)
            data = json.loads(request.body)
            
            # 2. 把数据写入刚才设计的数据库表格中
            SensorData.objects.create(
                temperature=data['temperature'],
                humidity=data['humidity']
            )
            
            # 3. 回复 ESP32："收到了！"
            return JsonResponse({"status": "success", "message": "Data saved!"})
            
        except Exception as e:
            # 如果出错 (比如发来的数据格式不对)，报错给它看
            return JsonResponse({"status": "error", "message": str(e)})
            
    return JsonResponse({"status": "error", "message": "Only POST allowed"})

# ---------------------------------------------------------
# 接口 2: 获取历史数据 (给 React 网页用)
# ---------------------------------------------------------
def get_history(request):
    # 1. 去数据库捞数据：按时间倒序(最新的在前面)，只取最近10条
    raw_data = SensorData.objects.all().order_by('-timestamp')[:10]
    
    # 2. 整理数据：把数据库对象转换成简单的字典列表
    result_list = []
    for item in raw_data:
        result_list.append({
            "temp": item.temperature,
            "humi": item.humidity,
            "time": item.timestamp.strftime("%H:%M:%S") # 把时间转成 "12:30:05" 这种格式
        })
    
    # 3. 把整理好的列表发回给网页
    return JsonResponse({"data": result_list})
