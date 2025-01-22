./.venv/Scripts/activate.bat
pyinstaller --distpath build .\python\get_paint.spec
pyinstaller --distpath build .\python\LSPaint.spec
pyinstaller --distpath build .\python\png_to_rgb.spec
pyinstaller --distpath build .\python\rgb_to_png.spec
pyinstaller --distpath build .\python\gen_value.spec