$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cards = Get-Content -LiteralPath (Join-Path $root 'data\cards\starters.json') -Raw | ConvertFrom-Json
if ($cards.Count -ne 32) { throw 'Expected 32 starter card definitions' }
foreach ($color in 0,1) {
    $deck = $cards | Where-Object { $_.color -eq $color }
    if (($deck | Where-Object kind -ne 0 | Measure-Object copies -Sum).Sum -ne 50) { throw 'Main deck must have 50 cards' }
    if (($deck | Where-Object kind -eq 0 | Measure-Object copies -Sum).Sum -ne 4) { throw 'Starter egg deck must have 4 cards' }
}
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('#include "game.h"')
$lines.Add('')
$lines.Add('const CardDef dg_cards[DG_DEFINITIONS] = {')
foreach ($card in $cards) {
    $number = ConvertTo-Json -InputObject $card.number -Compress
    $name = ConvertTo-Json -InputObject $card.name -Compress
    $displayText = $card.text.Normalize([Text.NormalizationForm]::FormKC)
    if ($displayText -match '[^\x00-\x7F]') { throw ('Unsupported text character: ' + $card.number) }
    $text = ConvertTo-Json -InputObject $displayText -Compress
    $lines.Add(('    {{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}}},' -f $number,$name,$text,$card.kind,$card.color,$card.level,$card.cost,$card.evo,$card.dp,$card.copies))
}
$lines.Add('};')
[IO.File]::WriteAllLines((Join-Path $root 'src\core\cards.c'), $lines, [Text.UTF8Encoding]::new($false))
Write-Output 'Generated 32 card definitions and validated both practice decks.'
