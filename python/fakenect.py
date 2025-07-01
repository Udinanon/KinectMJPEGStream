import freenect
from matplotlib import pyplot as plt
import numpy as np
import time

threshold = 100
current_depth = 1000
depth, timestamp = freenect.sync_get_depth()
depth = 255 * np.logical_and(depth >= current_depth - threshold,
                            depth <= current_depth + threshold)
depth = depth.astype(np.uint8)
#plt.imshow(depth)
#plt.show()


im=plt.imshow(depth)
while True:
    depth, timestamp = freenect.sync_get_depth()
    depth = 255 * np.logical_and(depth >= current_depth - threshold, depth <= current_depth + threshold)
    depth = depth.astype(np.uint8)
    im.set_data(depth)
    plt.pause(0.02)
plt.show()

'''
while True:
    depth, timestamp = freenect.sync_get_depth()
    depth = 255 * np.logical_and(depth >= current_depth - threshold, depth <= current_depth + threshold)
    depth = depth.astype(np.uint8)
    #print(np.sum(depth))
    plt.clf()
    plt.imshow(depth)
    time.sleep(1)
    '''