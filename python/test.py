import socket
import threading
import requests
import struct
import ipaddress
import json

count = 0
def start_server(port):
    global count
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_socket.bind(('192.168.86.247', port))
    print(f"[*] Listening on port {port}")
    
    while True:
        res = server_socket.recv(2024)
        print(res[0:res.find(0)])
        count += 1
        
def main():
    # Start the socket server in a separate thread
    server_port = 3001  # Change this to your desired port number

    server_thread = threading.Thread(target=start_server, args=(server_port,))
    server_thread.start()

    ipv_binary = socket.inet_pton(socket.AF_INET, '192.168.86.247')
    port_binary = struct.pack('!H', server_port)

    # Send POST request with IPv4 in IPv6 and port in binary
    url = 'http://192.168.86.248/add_client'
    payload = ipv_binary + port_binary
    print(payload)
    response = requests.post(url, data=payload)

    print("POST request sent with data:", payload)
    print("Response:", response.text)

    server_thread.join()
    
if __name__ == "__main__":
    main()
