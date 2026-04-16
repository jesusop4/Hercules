$enc = [System.Text.Encoding]::GetEncoding(1252)
$bytes = [System.IO.File]::ReadAllBytes("C:\Users\nKorea\Desktop\Harus\Hercules\npc\ragnayokai\harus\gerais\quest.txt")
for ($i = 0; $i -lt ($bytes.Length - 2); $i++) {
    if ($bytes[$i] -eq 0xEF -and $bytes[$i+1] -eq 0xBF -and $bytes[$i+2] -eq 0xBD) {
        $start = [Math]::Max(0, $i - 30)
        $len = [Math]::Min(80, $bytes.Length - $start)
        $ctx = $enc.GetString($bytes, $start, $len)
        Write-Host "Pos ${i}: $ctx"
    }
}
Write-Host "Done"
