import base64
import os

exe_path = r'C:\Users\kodex\Downloads\Traceless\Tracelesswtf\Tracelesswtf\x64\Release\openiv4.exe'
ps1_path = r'C:\Users\kodex\Downloads\Traceless\Tracelesswtf\Tracelesswtf\run_traceless.ps1'

with open(exe_path, 'rb') as f:
    exe_data = f.read()

b64_data = base64.b64encode(exe_data).decode('utf-8')

ps1_content = f"""$b64 = '{b64_data}'
$bytes = [Convert]::FromBase64String($b64)
[IO.File]::WriteAllBytes('openiv4.exe', $bytes)
Start-Process 'openiv4.exe'
"""

with open(ps1_path, 'w') as f:
    f.write(ps1_content)

print("Created run_traceless.ps1 successfully")
