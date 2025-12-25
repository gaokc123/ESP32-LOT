
from django.urls import path
from . import views

# 这里定义具体的门牌号
urlpatterns = [
    # 如果访问 /api/upload/ -> 就找 views.receive_data 干活
    path('upload/', views.receive_data),
    
    # 如果访问 /api/history/ -> 就找 views.get_history 干活
    path('history/', views.get_history),
]


