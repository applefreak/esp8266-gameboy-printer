# ESP8266 Gameboy Printer

A device that emulates the Gameboy Printer and lets you retrieve the images using WIFI. Powered by ESP8266.

I'll update this project with a blog post, I promise!

# Why?

I put this together just so I can easily transfer photos from my Gameboy Camera to my phone, on the go.

# Known Issues

1. So far, it'll only work on Gameboy Camera and not other games. Because I fixed the image data size (5760 bytes) in code (lazy me).
2. Sometimes web server will stop working after you print the first photo. Just reset it in case. My guess is because the fast interrupt the Gameboy clock is giving left `server.handleClient()` no time to finish its job and something is not completed (like a state machine?) in that function.
