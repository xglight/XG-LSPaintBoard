./.venv/Scripts/activate.bat
cd python
pyinstaller -y LSPaint.spec
pyinstaller -y png_to_rgb.spec
pyinstaller -y rgb_to_png.spec
pyinstaller -y GetPaint.spec