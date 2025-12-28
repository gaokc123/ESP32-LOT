from django.db import models

# Create your models here.

class SensorData(models.Model):
    # 只保留音频文件，允许为空以便于调试，实际使用中应该都有
    audio_file = models.FileField(upload_to='audio/', null=True, blank=True)
    
    # 自动记录上传时间
    timestamp = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return f"Record at {self.timestamp}"