param(
    [string]$OutputDir = "main/assets",
    [string]$FontName = "Microsoft YaHei UI",
    [int]$GlyphWidth = 12,
    [int]$GlyphHeight = 12
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

function New-GlyphBitmapBytes {
    param(
        [int]$CodePoint,
        [System.Drawing.Font]$Font,
        [int]$Width,
        [int]$Height
    )

    $bmp = New-Object System.Drawing.Bitmap $Width, $Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $text = [char]::ConvertFromUtf32($CodePoint)
    $g.DrawString($text, $Font, [System.Drawing.Brushes]::White, -1, -2)

    $bytesPerRow = [Math]::Ceiling($Width / 8.0)
    $bytes = New-Object byte[] ($bytesPerRow * $Height)

    for ($y = 0; $y -lt $Height; $y++) {
        for ($x = 0; $x -lt $Width; $x++) {
            $pixel = $bmp.GetPixel($x, $y)
            if ($pixel.R -gt 0) {
                $byteIndex = $y * $bytesPerRow + [Math]::Floor($x / 8)
                $bitIndex = 7 - ($x % 8)
                $bytes[$byteIndex] = $bytes[$byteIndex] -bor (1 -shl $bitIndex)
            }
        }
    }

    $g.Dispose()
    $bmp.Dispose()
    return ,$bytes
}

function Write-RangeFile {
    param(
        [int]$Start,
        [int]$End,
        [string]$Path,
        [System.Drawing.Font]$Font,
        [int]$Width,
        [int]$Height
    )

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        for ($cp = $Start; $cp -le $End; $cp++) {
            $glyphBytes = New-GlyphBitmapBytes -CodePoint $cp -Font $Font -Width $Width -Height $Height
            $stream.Write($glyphBytes, 0, $glyphBytes.Length)
        }
    }
    finally {
        $stream.Dispose()
    }
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$font = New-Object System.Drawing.Font($FontName, $GlyphHeight, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
try {
    Write-Host "Generating CJK bitmap font assets into $OutputDir"
    Write-RangeFile -Start 0x3000 -End 0x303F -Path (Join-Path $OutputDir "cjk_3000_303f_12.bin") -Font $font -Width $GlyphWidth -Height $GlyphHeight
    Write-RangeFile -Start 0x3400 -End 0x9FFF -Path (Join-Path $OutputDir "cjk_3400_9fff_12.bin") -Font $font -Width $GlyphWidth -Height $GlyphHeight
    Write-RangeFile -Start 0xFF00 -End 0xFFEF -Path (Join-Path $OutputDir "cjk_ff00_ffef_12.bin") -Font $font -Width $GlyphWidth -Height $GlyphHeight
    Write-Host "Done"
}
finally {
    $font.Dispose()
}
