import math
import socket
import select

duty = 0
stop = False
img = "a"
sock = socket.socket()
sock.connect(('192.168.0.135', 80))
sock.setblocking(False)
def process(data):
    if data == "A," or data == "A" :
        print("You pressed: a")
    elif data == "B," or data == "B":
        print("You pressed: b")
    elif data == "X," or data == "X":
        print("You pressed: x")
    elif data == "Y," or data == "Y":
        print("You pressed: y")
    elif data == "SELECT," or data == "SELECT":
        print("You pressed: select")
    elif data == "u," or data == "u":
        print("You pressed: up")
    elif data == "d," or data == "d":
        print("You pressed: down")


while True:


    read_connections, write_connections, e = select.select([sock], [sock], [])
    for connection in read_connections:
        data = connection.recv(256).decode("ascii")
        if data == "":
            print("poka-poka")
            break
        process(data)
        if data.startswith('0') and data.find(',',0,len(data)) == -1:
            print('stopped')
        elif data.find(';',0,len(data)) != -1:
            data = data.split(',')
            if data[len(data)-1]==('0'):
                stop = True
                data.remove('0')
            elif data[len(data)-1]==(''):
                data.remove('')
            else:
                data.pop()
            f = 0
            while f < len(data):
                if data[f].find(';',0,len(data[f])) == -1:
                    try:
                        process(data[f])
                    except:
                        continue
                    data.pop(f)
                    f -= 1
                f += 1
            dlin = len(data)
            for i in range(dlin):
                data_n = data[i]
                data_n = data_n.split(';')
                datay = data_n[0]
                datax = data_n[1]
                #print(f'x: {datax};  y:{datay}')
                try:
                    datax = int(datax)
                    datay = int(datay)
                    if (datax > 25 or datax < -25) or (datay > 25 or datay < -25): 
                        if abs(datax) > abs(datay):
                            if datax > 0:
                                datax -= 25
                                duty = math.floor(datax/6.4)
                                print("Straight: ",duty)
                            else:
                                datax = abs(datax)
                                datax -= 25
                                duty = math.floor(datax/6.4)
                                print("Backwards: ",duty)
                        else:
                            if datay > 0:
                                datay -= 25
                                duty = math.floor(datay/6.4)
                                print("Right: ",duty)
                            else:
                                datay = abs(datay)
                                datay -= 25
                                duty = math.floor(datay/6.4)
                                print("Left: ",duty)
                except:
                    continue
            if stop == True:
                print('stoped')
                stop = False
    #for connection in write_connections:
        #connection.send(img.encode("ascii"))
    
   
                


sock.close()

