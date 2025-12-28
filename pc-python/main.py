import asyncio
import websockets
import struct
import socket # <--- Lägg till denna för att hämta IP
import matplotlib.pyplot as plt
import numpy as np




# Inställningar - se till att porten matchar den i din ESP32-kod
IP = "0.0.0.0" # Lyssna på alla nätverkskort
PORT = 8080

# Struct-format: 
# < = Little Endian (standard för ESP32)
# f = float (4 bytes, vinkel)
# H = unsigned short (2 bytes, distans)
# Vi förväntar oss 100 stycken av dessa i rad
LIDAR_PACKET_LENGTH = 100
MEASUREMENT_SIZE = 6 # 4 + 2 bytes
EXPECTED_SIZE = LIDAR_PACKET_LENGTH * MEASUREMENT_SIZE




def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Vi behöver inte ens skicka något, detta öppnar bara interfacet
        s.connect(('8.8.8.8', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

async def handler(websocket):
    print(f"ESP32 ansluten från: {websocket.remote_address}")
    try:
        async for message in websocket:
            # Kontrollera att vi fått rätt mängd data
            if len(message) == EXPECTED_SIZE:
                # Packa upp hela batchen
                # "100fH" betyder: repetera (float, uint16) 100 gånger
                format_str = "<" + "fH" * LIDAR_PACKET_LENGTH
                data = struct.unpack(format_str, message)

                # Dela upp datan (varannan är vinkel, varannan distans)
                angles = data[0::2]    # Börja på 0, hoppa 2
                distances = data[1::2] # Börja på 1, hoppa 2



                xpoints = np.array(distances) * np.cos(np.radians(angles))
                ypoints = np.array(distances) * np.sin(np.radians(angles))

                plt.plot(xpoints, ypoints)
                plt.show()


                # Skriv ut första mätningen i paketet som exempel
                print(f"Mottaget paket! Första mätning: Vinkel: {angles[0]:.2f}°, Distans: {distances[0]} cm")
            else:
                print(f"Oväntad datastorlek: {len(message)} bytes")

    except websockets.ConnectionClosed:
        print("ESP32 kopplade ifrån.")

async def main():
    local_ip = get_local_ip()
    print("="*40)
    print(f"LIDAR SERVER STARTAD")
    print(f"Lokal IP: {local_ip}")
    print(f"I din ESP32-kod, använd: ws://{local_ip}:{PORT}")
    print("="*40)
    
    async with websockets.serve(handler, IP, PORT):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())