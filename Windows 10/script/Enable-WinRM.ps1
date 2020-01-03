Set-NetConnectionProfile -NetworkCategory Private

Enable-PSRemoting -Force
winrm quickconfig -q
winrm set winrm/config/service '@{AllowUnencrypted="true"}'
winrm set winrm/config/service/auth '@{Basic="true"}'
Restart-Service winrm
