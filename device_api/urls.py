from django.urls import path
from . import views

urlpatterns = [
    # 这里的第一个参数为空 ''，代表应用的首页
    path('', views.index_page, name='index'), 
    path('api/upload/', views.receive_data, name='api_upload'),
    path('api/history/', views.get_history, name='get_history'),
    path('api/get_fft_by_id/', views.get_fft_by_id, name='get_fft_by_id'),
    path('api/get_audio_data/', views.get_audio_data, name='get_audio_data'),
]