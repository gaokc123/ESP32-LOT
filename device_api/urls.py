from django.urls import path
from . import views

urlpatterns = [
    # 你的单片机代码里写的 url 是 "/api/upload/"
    path('api/upload/', views.receive_data, name='api_upload'), 
    
    # 获取历史记录 (你原来的接口)
    path('api/history/', views.get_history, name='get_history'),
    
    # 网页 ECharts 获取频谱数据的新接口
    path('api/get_audio_data/', views.get_audio_data, name='get_audio_data'),
    
    # 网页界面 (确保之前给你写的 HTML 代码放在了 templates/index.html 里面)
    path('', views.index_page, name='index'), 
]