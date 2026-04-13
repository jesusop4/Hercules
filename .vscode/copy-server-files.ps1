$src = 'C:\Users\nKorea\Desktop\Harus\Hercules'
$dst = 'C:\Users\nKorea\Desktop\Harus\Hercules'

if ((Resolve-Path $src).Path -eq (Resolve-Path $dst).Path) {
    Write-Host 'Origem e destino sao a mesma pasta. Nada a copiar.'
    exit 0
}

Write-Host 'Copiando arquivos do servidor...'

Copy-Item -Path "$src\*.exe" -Destination $dst -Force
Copy-Item -Path "$src\*.dll" -Destination $dst -Force

foreach ($folder in @('conf','db','npc','data','maps','plugins','save','log')) {
    $s = Join-Path $src $folder
    $d = Join-Path $dst $folder
    if (Test-Path $s) {
        robocopy $s $d /E /NFL /NDL /NJH /NJS | Out-Null
        Write-Host " -> $folder OK"
    }
}

Write-Host 'Copia concluida.'
