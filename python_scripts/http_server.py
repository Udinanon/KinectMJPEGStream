import freenect
import numpy as np
import sys
import signal
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from PIL import Image  # Pillow library (lightweight)
import io
FPS = 60
HOVER_VALUE = -1

class MJPEGHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        print("Connected")
        self.send_response(200)

        self.active = True
        while self.active:
            if self.path == "/data":

                data = json.dumps(freenect.sync_get_depth()[0].tolist())
                try:
                    self.send_header('Content-type', 'application/json')
                    self.send_header('Model', 'mediapipe+demo')
                    self.end_headers()

                    self.wfile.write(data.encode())
                    self.active = False
                except BrokenPipeError or ConnectionResetError:
                    # Catch Pipe fails or Resets if client disconnects
                    print("connection ended")
                    self.active = False
            if self.path == "/rgb_once":
                    
                rgb_frame, _ = freenect.sync_get_video() 

                # Convert to JPEG using Pillow
                # Load image and convert to RGB
                img = Image.fromarray(rgb_frame)
                rgb_img = img.convert("RGB")
                # Create empty buffer to srite JPEG encoded image
                jpeg_buffer = io.BytesIO()
                # Save and read back
                rgb_img.save(jpeg_buffer, format="JPEG", quality=85)
            
                jpeg_data = jpeg_buffer.getvalue()

                self.send_header("Content-Type", "image/jpeg")
                self.send_header("Content-Length", len(jpeg_data))
                self.end_headers()
                self.wfile.write(jpeg_data)
                self.active = False
            if self.path == "/depth_once":
                    
                depth_frame, _ = freenect.sync_get_depth()
                    # Reshape depth frame from 2D to flat 3D: (x, y) -> (x, y, 1)
                    #depth_frame=depth_frame.reshape(depth_frame.shape[0], depth_frame.shape[1], 1)
                    #depth_3D = np.zeros_like(depth_frame)
                    #result = np.array([depth_to_metric(*idx, depth_frame[idx] * 2) for idx in np.ndindex(depth_frame.shape)])
                    # Mask out pixels by distance, then normalize and combine
                    # Broadcast to uint8 as floats are not appreciated by Pillow
                    #depth_frame[depth_frame > 3000] = 0
                normalized_depth_frame = (depth_frame - depth_frame.min())/(depth_frame.max()) * 255

                # Convert to JPEG using Pillow
                # Load image and convert to RGB
                img = Image.fromarray(normalized_depth_frame)
                depth_img = img.convert("RGB")
                # Create empty buffer to srite JPEG encoded image
                jpeg_buffer = io.BytesIO()
                # Save and read back
                depth_img.save(jpeg_buffer, format="JPEG", quality=85)
            
                jpeg_data = jpeg_buffer.getvalue()

                self.send_header("Content-Type", "image/jpeg")
                self.send_header("Content-Length", len(jpeg_data))
                self.end_headers()
                self.wfile.write(jpeg_data)
                self.active = False

            else:
                self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
                self.end_headers()

                if self.path == "/depth":
                # Collect RGB and Depth frames from Kinect
                    #depth_frame, _ = freenect.sync_get_depth(format=freenect.DEPTH_MM)
                    depth_frame, _ = freenect.sync_get_depth()
                    # Reshape depth frame from 2D to flat 3D: (x, y) -> (x, y, 1)
                    #depth_frame=depth_frame.reshape(depth_frame.shape[0], depth_frame.shape[1], 1)
                    #depth_3D = np.zeros_like(depth_frame)
                    #result = np.array([depth_to_metric(*idx, depth_frame[idx] * 2) for idx in np.ndindex(depth_frame.shape)])
                    # Mask out pixels by distance, then normalize and combine
                    # Broadcast to uint8 as floats are not appreciated by Pillow
                    #depth_frame[depth_frame > 3000] = 0
                    normalized_depth_frame = (depth_frame - depth_frame.min())/(depth_frame.max()) * 255
                    jpeg_buffer = io.BytesIO()
                    img = Image.fromarray(normalized_depth_frame)
                    depth_img = img.convert("RGB")
                    depth_img.save(jpeg_buffer, format="JPEG", quality=85)
                if self.path == "/rgb":
                    #masked_rgb_frame = (rgb_frame * normalized_depth_frame).astype(np.uint8)
                    rgb_frame, _ = freenect.sync_get_video() 

                    # Convert to JPEG using Pillow
                    # Load image and convert to RGB
                    img = Image.fromarray(rgb_frame)
                    rgb_img = img.convert("RGB")
                    # Create empty buffer to srite JPEG encoded image
                    jpeg_buffer = io.BytesIO()
                    # Save and read back
                    rgb_img.save(jpeg_buffer, format="JPEG", quality=85)
                if self.path == "/":
                    depth_frame, _ = freenect.sync_get_depth()
                    normalized_depth_frame = (depth_frame - depth_frame.min())/(depth_frame.max()) * 255
                    #depth_frame=depth_frame.reshape(depth_frame.shape[0], depth_frame.shape[1], 3)
                    rgb_frame, _ = freenect.sync_get_video()

                    jpeg_buffer = io.BytesIO()
                    img = Image.fromarray(rgb_frame)

                    rgb_img = img.convert("RGB")

                    img = Image.fromarray(normalized_depth_frame)
                    depth_img = img.convert("RGB")

                    w = rgb_img.size[0] + depth_img.size[0]
                    h = max(rgb_img.size[1], depth_img.size[1])
                    im = Image.new("RGB", (w, h))

                    im.paste(rgb_img)
                    im.paste(depth_img, (rgb_img.size[0], 0))
                    im.save(jpeg_buffer, format="JPEG", quality=85)


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

MPEG_Server = ThreadingHTTPServer(("0.0.0.0", 8080), MJPEGHandler)
def stop_server(signal, frame):
    MPEG_Server.server_close()
    print("Server stopped")
    sys.exit(0)

signal.signal(signal.SIGINT, stop_server)
print("Starting Server")
MPEG_Server.serve_forever()
