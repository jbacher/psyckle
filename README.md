# Psyckle #

Resize images with nginx and OpenCV.

### Wat ###

* An nginx module to resize images on the fly with nginx. Similar to http://nginx.org/en/docs/http/ngx_http_image_filter_module.html, but using OpenCV and using query parameters instead of static configurations.

### How do I get set up? ###

*You'll need copy of the nginx source http://nginx.org/en/download.html
* You'll need a copy of OpenCV 3.0 (alpha) http://opencv.org/
* You'll want to build OpenCV first (it uses cmake)
* After that, build nginx (and pass --add-module=/path/to/psyckle)

### Who do I talk to? ###

* Your face
* Connor