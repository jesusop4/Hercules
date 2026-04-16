# gen_mapcache.ps1 — gera eden.mcache a partir de eden.gat
# Usa zlib1.dll (já presente em Hercules) para comprimir no formato zlib

param(
    [string]$MapName = "eden",
    [string]$Root    = "c:\Users\nKorea\Desktop\Harus\Hercules"
)

$gatPath    = Join-Path $Root "data\$MapName.gat"
$mcachePath = Join-Path $Root "maps\re\$MapName.mcache"
$zlibDll    = Join-Path $Root "zlib1.dll"

if (-not (Test-Path $gatPath))  { Write-Error "GAT nao encontrado: $gatPath"; exit 1 }
if (-not (Test-Path $zlibDll))  { Write-Error "zlib1.dll nao encontrado: $zlibDll"; exit 1 }

# ── P/Invoke para compress2 da zlib1.dll ─────────────────────────────────────
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ZLib {
    [DllImport(@"$($zlibDll.Replace('\','\\'))", CallingConvention = CallingConvention.Cdecl)]
    public static extern int compress2(byte[] dest, ref ulong destLen, byte[] source, ulong sourceLen, int level);
}
"@

# ── Lê o GAT ─────────────────────────────────────────────────────────────────
function GetULong([byte[]]$buf, [int]$offset) {
    return [uint32]($buf[$offset] -bor ($buf[$offset+1] -shl 8) -bor ($buf[$offset+2] -shl 16) -bor ($buf[$offset+3] -shl 24))
}
function GetFloat([byte[]]$buf, [int]$offset) {
    return [System.BitConverter]::ToSingle($buf, $offset)
}

$gat = [System.IO.File]::ReadAllBytes($gatPath)

if ([System.Text.Encoding]::ASCII.GetString($gat[0..3]) -ne "GRAT") {
    Write-Error "Arquivo nao e um GAT valido"; exit 1
}

$xs = [int](GetULong $gat 6)
$ys = [int](GetULong $gat 10)
Write-Host "Mapa: $MapName  |  xs=$xs  ys=$ys  |  cells=$($xs*$ys)"

# ── Monta array de cells (1 byte por cell = tipo) ────────────────────────────
$mapSize = $xs * $ys
$cells   = [byte[]]::new($mapSize)

for ($xy = 0; $xy -lt $mapSize; $xy++) {
    $base = $xy * 20
    # height está em offset 14 do cursor que começa em gat[0]:
    # => gat[base + 14], type em gat[base + 30]
    $height = GetFloat $gat ($base + 14)
    $type   = GetULong  $gat ($base + 30)
    # sem RSW => sem water_height => nunca forçamos type=3
    $cells[$xy] = [byte]($type -band 0xFF)
}

# ── Comprime com zlib (compress2) ────────────────────────────────────────────
$destLen  = [UInt64]($mapSize * 2 + 64)
$destBuf  = [byte[]]::new($destLen)
$ret = [ZLib]::compress2($destBuf, [ref]$destLen, $cells, [UInt64]$mapSize, 6)
if ($ret -ne 0) { Write-Error "compress2 falhou: $ret"; exit 1 }

$compressed = $destBuf[0..([int]$destLen - 1)]
Write-Host "Comprimido: $($compressed.Length) bytes"

# ── MD5 dos dados comprimidos ─────────────────────────────────────────────────
$md5     = [System.Security.Cryptography.MD5]::Create()
$md5hash = $md5.ComputeHash($compressed)  # 16 bytes

# ── Monta o header do .mcache (packed struct) ────────────────────────────────
# int16 version | uint8[16] md5 | int16 xs | int16 ys | int32 len
$header = [System.Collections.Generic.List[byte]]::new()
# version = 1 (int16 LE)
$header.Add([byte]0x01); $header.Add([byte]0x00)
# md5 (16 bytes)
foreach ($b in $md5hash) { $header.Add($b) }
# xs (int16 LE)
$header.Add([byte]($xs -band 0xFF)); $header.Add([byte](($xs -shr 8) -band 0xFF))
# ys (int16 LE)
$header.Add([byte]($ys -band 0xFF)); $header.Add([byte](($ys -shr 8) -band 0xFF))
# len (int32 LE)
$clen = [int]$destLen
$header.Add([byte]($clen -band 0xFF))
$header.Add([byte](($clen -shr 8)  -band 0xFF))
$header.Add([byte](($clen -shr 16) -band 0xFF))
$header.Add([byte](($clen -shr 24) -band 0xFF))

# ── Escreve o .mcache ─────────────────────────────────────────────────────────
$out = [System.Collections.Generic.List[byte]]::new()
foreach ($b in $header) { $out.Add($b) }
foreach ($b in $compressed) { $out.Add($b) }

[System.IO.File]::WriteAllBytes($mcachePath, $out.ToArray())
Write-Host "Gerado: $mcachePath ($($out.Count) bytes)"
