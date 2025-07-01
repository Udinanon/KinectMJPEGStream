import freenect
import numpy as np
import sys
import signal
from http.server import BaseHTTPRequestHandler, HTTPServer
from PIL import Image  # Pillow library (lightweight)
import io
FPS = 60
HOVER_VALUE = -1

class MJPEGHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        print("Connected")
        self.send_response(200)
        self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
        self.end_headers()
        self.active = True
        while self.active:
            # Collect RGB and Depth frames from Kinect
            rgb_frame, _ = freenect.sync_get_video() 
            depth_frame, _ = freenect.sync_get_depth(format=freenect.DEPTH_MM)
            # Reshape depth frame from 2D to flat 3D: (x, y) -> (x, y, 1)
            depth_frame=depth_frame.reshape(depth_frame.shape[0], depth_frame.shape[1], 1)
            
            # Mask out pixels by distance, then normalize and combine
            # Broadcast to uint8 as floats are not appreciated by Pillow
            depth_frame[depth_frame > 3000] = 0
            normalized_depth_frame = (depth_frame - depth_frame.min())/(depth_frame.max())
            masked_rgb_frame = (rgb_frame * normalized_depth_frame).astype(np.uint8)
            
            # Convert to JPEG using Pillow
            # Load image and convert to RGB
            img = Image.fromarray(masked_rgb_frame)
            rgb_img = img.convert("RGB")
            # Create empty buffer to srite JPEG encoded image
            jpeg_buffer = io.BytesIO()
            # Save and read back
            rgb_img.save(jpeg_buffer, format="JPEG", quality=85)
            jpeg_data = jpeg_buffer.getvalue()

            # Stream over HTTP
            try:
                self.wfile.write(b"--frame\r\n")
                self.send_header("Content-Type", "image/jpeg")
                self.send_header("Content-Length", len(jpeg_data))
                self.end_headers()
                self.wfile.write(jpeg_data)
            except BrokenPipeError or ConnectionResetError:
                # Catch Pipe fails or Resets if client disconnects
                print("connection ended")
                self.active = False

# Start server

MPEG_Server = HTTPServer(("0.0.0.0", 8080), MJPEGHandler)
def stop_server(signal, frame):
    MPEG_Server.server_close()
    print("Server stopped")
    sys.exit(0)

signal.signal(signal.SIGINT, stop_server)
print("Starting Server")
MPEG_Server.serve_forever()
