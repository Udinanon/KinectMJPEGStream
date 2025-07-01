import freenect
import numpy as np

import matplotlib.pyplot as plt
import matplotlib as mpl
FPS = 60
HOVER_VALUE = -1
def convolve2d_fft(image, kernel):
    # Get the size of the output
    output_shape = (image.shape[0] + kernel.shape[0] - 1, image.shape[1] + kernel.shape[1] - 1)

    # Perform FFT on both the image and the kernel
    image_fft = np.fft.fft2(image, s=output_shape)
    kernel_fft = np.fft.fft2(kernel, s=output_shape)

    # Multiply in the frequency domain
    result_fft = image_fft * kernel_fft

    # Inverse FFT to get the convolved image
    result = np.fft.ifft2(result_fft).real

    return result


# https://stackoverflow.com/questions/2448015/2d-convolution-using-python-and-numpy
def framed(image, width):
    a = np.zeros((image.shape[0]+2*width, image.shape[1]+2*width))
    a[width:-width, width:-width] = image
    # alternatively fill the frame with ones or copy border pixels
    return a

def slow_conv2d(image, kernel):
    # apply kernel to image, return image of the same shape
    # assume both image and kernel are 2D arrays
    # kernel = np.flipud(np.fliplr(kernel))  # optionally flip the kernel
    k = kernel.shape[0]
    width = k//2
    # place the image inside a frame to compensate for the kernel overlap
    a = framed(image, width)
    b = np.zeros(image.shape)  # fill the output array with zeros; do not use np.empty()
    # shift the image around each pixel, multiply by the corresponding kernel value and accumulate the results
    for p, dp, r, dr in [(i, i + image.shape[0], j, j + image.shape[1]) for i in range(k) for j in range(k)]:
        b += a[p:dp, r:dr] * kernel[p, r]
    # or just write two nested for loops if you prefer
    # np.clip(b, 0, 255, out=b)  # optionally clip values exceeding the limits
    return b

def on_mouse_move(event):
    if event.inaxes:
        global depth_matrix
        global HOVER_VALUE
        HOVER_VALUE = depth_matrix[int(event.ydata), int(event.xdata)]

def on_close(event):
    print(f"{event = }")
    global running
    running = False

plt.style.use('dark_background')

fig_depth, axs_depth = plt.subplots(1, 2)
cmap = mpl.colormaps["plasma"]

sm = plt.cm.ScalarMappable(cmap=cmap)
sm.set_array([])
colorbar = fig_depth.colorbar(sm,
             ax=axs_depth[0], label="Normalized Thrust [a.u.]")

running = True
fig_depth.canvas.mpl_connect('close_event', on_close)
fig_depth.canvas.mpl_connect('motion_notify_event', on_mouse_move)

edge_matrix = np.array([[-1, -1, -1], [-1, 8, -1], [-1, -1, -1]])

# Initialize the images
#global depth_matrix
tmp_depth_matrix = np.ones((480, 640))  # Replace with your actual dimensions
tmp_edge_detected = np.zeros((480, 640))  # Replace with your actual dimensions
img_depth = axs_depth[0].imshow(tmp_depth_matrix, cmap=cmap)
img_edge = axs_depth[1].imshow(tmp_edge_detected, cmap='grey')  # Use appropriate colormap


while running:    
    depth_matrix, timestamp = freenect.sync_get_depth(format=freenect.DEPTH_MM)

     # Update the depth image
    img_depth.set_data(depth_matrix)
    img_depth.set_clim(vmin=depth_matrix.min(), vmax=depth_matrix.max())  # Update color limits if necessary
    sm.set_clim(vmin=depth_matrix.min(), vmax=depth_matrix.max())
    #print(f"{depth_matrix =}")
    #print(f"{edge_matrix =}")
    
    edge_detected = convolve2d_fft(depth_matrix, edge_matrix)
    img_edge.set_data(edge_detected)
    img_edge.set_clim(vmin=depth_matrix.min(), vmax=depth_matrix.max())  # Update color limits if necessary
   
    # Update titles if needed
    axs_depth[0].set_title(f"Depth Image - Depth: {HOVER_VALUE}")
    axs_depth[1].set_title("Edge Detected Image")
    plt.draw()
    plt.pause(1/FPS)
plt.show()