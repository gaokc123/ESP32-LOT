import os
import django

# 1. 设置 Django 环境
os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'core.settings')
django.setup()

# 2. 导入模型
from device_api.models import SensorData

# 3. 执行删除
def clear_all():
    print("正在连接数据库...")
    count, _ = SensorData.objects.all().delete()
    print(f"✅ 成功删除了 {count} 条记录！")

if __name__ == "__main__":
    clear_all()