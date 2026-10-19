import subprocess
import sys
import time

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 integration_test.py <server> <client>")
        return 1
    #定义服务端客户端路径
    server_path = sys.argv[1]
    client_path = sys.argv[2]

    
    with open("server.log","w+") as log:
        #启动服务端
        server = subprocess.Popen([server_path,"8888"],stdout=log,stderr=log)
        try:
            time.sleep(0.3)
            #启动客户端
            result = subprocess.run([client_path,"127.0.0.1","8888"],
                                    input="hello\n/quit\n",
                                    text=True,
                                    capture_output=True,
                                    timeout=5)
            # 4. 查看客户端运行结果
            content=""
            for _ in range(50):
                log.flush()
                log.seek(0)
                content = log.read()
                if "hello\n" in content:
                    break
                time.sleep(0.1)
            if result.returncode == 0 and "hello\n" in content:
                print("PASS: TCP communication successful")
            else:
                print("FAIL: TCP communication failed")
                print("Client returncode:", result.returncode)
                print("Server log:", content)
                raise AssertionError("TCP communication test failed")
        finally:
            # 5. 无论测试成功还是失败，都清理服务端进程
            if server.poll() is None:
                server.terminate()
            try:
                server.wait(timeout=3)    #等待3秒退出
            except subprocess.TimeoutExpired: #如果3秒后没有退出
                server.kill()  #强制杀死进程
                server.wait()  #回收僵尸进程

if __name__ == "__main__":
    sys.exit(main())




