local http = require 'http'
function lovr.load()


    --local status, data, headers = http.request("0.0.0.0:8080/rgb_once")
    local status, data, headers = http.request("0.0.0.0:8888")
    --local parsed_data = json.decode(data)
    local image = lovr.data.newImage(lovr.data.newBlob(data))
    texture = lovr.graphics.newTexture(image, {mipmaps = false, usage = {"sample", "transfer"}})
end

function lovr.draw(pass)
    pass:setColor(1, 0, 0)
    pass:line(0, 0, 0, 1, 0, 0)
    pass:setColor(0, 1, 0)
    pass:line(0, 0, 0, 0, 1, 0)
    pass:setColor(0, 0, 1)
    pass:line(0, 0, 0, 0, 0, 1)
    pass:setColor(1, 1, 1, 1)
    --local status, data, headers = http.request("0.0.0.0:8080/depth_once")
    local status, data, headers = http.request("0.0.0.0:8888")
    --local parsed_data = json.decode(data)
    local image = lovr.data.newImage(lovr.data.newBlob(data))
    texture:setPixels(image)
    pass:setMaterial(texture)
    pass:cube()
end