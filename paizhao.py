import sensor,time
import image
import uos

# 初始化摄像头
sensor.reset()                      # 重置感光元件
sensor.set_pixformat(sensor.RGB565) # 设置图像格式为RGB565（彩色）
sensor.set_framesize(sensor.QVGA)   # 设置图像大小（320x240）
sensor.skip_frames(time=2000)       # 等待设置生效（跳过2秒帧）

for num in range(120):

    time.sleep_ms(1000)

    # 捕获图像
    img = sensor.snapshot()             # 拍摄一张照片


    # 保存到SD卡（确保SD卡已插入）
    try:
        # 保存为JPEG格式（路径以"/sd/"开头）
        strName = "youhailaji333"+str(num)+".jpg"
        img.save(strName, quality=95)  # quality控制JPEG质量(1-100)
        print("文件验证成功")

        ## 可选：检查文件是否存在
        #if strName in uos.listdir("/sd/images"):
            #print("文件验证成功")
        #else:
            #print("文件保存失败")

    except Exception as e:
        print("保存失败:", e)
