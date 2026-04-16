$bytes = [System.IO.File]::ReadAllBytes('c:\Users\nKorea\Desktop\Harus\Hercules\npc\ragnayokai\harus\gerais\lootconfig.txt')
$search = [byte[]]@(0x43,0x6F,0x6E,0x66,0x69,0x67,0x75,0x72,0x61)  # "Configura"
for ($i = 0; $i -lt $bytes.Length - 14; $i++) {
    $match = $true
    for ($k = 0; $k -lt $search.Length; $k++) {
        if ($bytes[$i+$k] -ne $search[$k]) { $match = $false; break }
    }
    if ($match) {
        $hex = ($bytes[$i..($i+13)] | ForEach-Object { $_.ToString('X2') }) -join ' '
        Write-Host "Pos ${i}: $hex"
    }
}
