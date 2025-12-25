from django.db import models

# Create your models here.
from django.db import models

# Create your models here.

# 想象我们在设计一张 Excel 表格，表名叫 SensorData
class SensorData(models.Model):
    # 第一列：温度 (浮点数，比如 25.5)
    temperature = models.FloatField()
    
    # 第二列：湿度 (浮点数，比如 60.2)
    humidity = models.FloatField()
    
    # 第三列：时间 (自动记录当前时间，不需要手动填)
    timestamp = models.DateTimeField(auto_now_add=True)

    # 这是一个方便给人类看的功能
    # 当你在后台查看数据时，它会显示 "时间 - 温度" 这样的格式，而不是 "Object(1)"
    def __str__(self):
        return f"{self.timestamp} - Temp: {self.temperature}"
    
