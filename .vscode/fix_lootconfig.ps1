# fix_lootconfig.ps1 — converte lootconfig.txt para Windows-1252
$outPath = "c:\Users\nKorea\Desktop\Harus\Hercules\npc\ragnayokai\harus\gerais\lootconfig.txt"

$bytes = [System.IO.File]::ReadAllBytes($outPath)

# --- PRE-PASSE: padroes especificos de bytes conhecidos ---
# "Configuracao": cada byte Windows-1252 em 0xC0-0xFF vira FFFD isolado quando lido como UTF-8
# c (0xE7) + a (0xE3) + o (0x6F) -> EF BF BD + EF BF BD + 6F
# padrao: 61(a) FFFD FFFD 6F(o) => 61(a) E7(c) E3(a) 6F(o)
$pat = [byte[]]@(0x61, 0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD, 0x6F)
$rep = [byte[]]@(0x61, 0xE7, 0xE3, 0x6F)
$tmp = [System.Collections.Generic.List[byte]]::new()
$j = 0
while ($j -lt $bytes.Length) {
    $matched = $false
    if ($j -le $bytes.Length - $pat.Length) {
        $ok = $true
        for ($k = 0; $k -lt $pat.Length; $k++) {
            if ($bytes[$j+$k] -ne $pat[$k]) { $ok = $false; break }
        }
        if ($ok) {
            foreach ($rb in $rep) { $tmp.Add($rb) }
            $j += $pat.Length
            $matched = $true
        }
    }
    if (-not $matched) { $tmp.Add($bytes[$j]); $j++ }
}
$bytes = $tmp.ToArray()
# --- FIM DO PRE-PASSE ---

$result = [System.Collections.Generic.List[byte]]::new()
$i = 0

# Strip UTF-8 BOM (EF BB BF) if present
if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF `
    -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
    $i = 3
    Write-Host "BOM removido"
}

while ($i -lt $bytes.Length) {
    $b = $bytes[$i]

    # U+FFFD replacement char (EF BF BD) — era provavelmente »
    if ($b -eq 0xEF -and ($i+2) -lt $bytes.Length `
        -and $bytes[$i+1] -eq 0xBF -and $bytes[$i+2] -eq 0xBD) {
        $result.Add([byte]0xBB)   # »
        $i += 3
    }
    # em dash U+2014 (E2 80 94) → Windows-1252 0x97
    elseif ($b -eq 0xE2 -and ($i+2) -lt $bytes.Length `
        -and $bytes[$i+1] -eq 0x80 -and $bytes[$i+2] -eq 0x94) {
        $result.Add([byte]0x97)
        $i += 3
    }
    # C2 XX → U+0080-U+00BF → byte Latin-1 = 0x80|(XX&0x3F)
    elseif ($b -eq 0xC2 -and ($i+1) -lt $bytes.Length `
        -and $bytes[$i+1] -ge 0x80 -and $bytes[$i+1] -le 0xBF) {
        $result.Add([byte](($bytes[$i+1] -band 0x3F) -bor 0x80))
        $i += 2
    }
    # C3 XX → U+00C0-U+00FF → byte Latin-1 = 0xC0|(XX&0x3F)
    elseif ($b -eq 0xC3 -and ($i+1) -lt $bytes.Length `
        -and $bytes[$i+1] -ge 0x80 -and $bytes[$i+1] -le 0xBF) {
        $result.Add([byte](($bytes[$i+1] -band 0x3F) -bor 0xC0))
        $i += 2
    }
    else {
        $result.Add($b)
        $i++
    }
}

[System.IO.File]::WriteAllBytes($outPath, $result.ToArray())

# Verificacao
$final = [System.IO.File]::ReadAllBytes($outPath)
$bad   = 0
for ($j = 0; $j -lt $final.Length; $j++) {
    if ($final[$j] -eq 0xEF -or $final[$j] -eq 0xC2 -or $final[$j] -eq 0xC3) { $bad++ }
}
Write-Host "Pronto: $($final.Length) bytes | Suspeitos restantes: $bad"
