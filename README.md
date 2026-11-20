# Building DigiTCGB

Download and extract [GBDK 2020 4.5.0](https://github.com/gbdk-2020/gbdk-2020/releases/tag/4.5.0).

Open PowerShell in this folder and run the command below, replacing the compiler path with your GBDK location:

```powershell
./tools/build.ps1 -Compiler 'path/to/gbdk/bin/lcc.exe'
```

The finished ROM is written to `build/DigiTCGB.gbc`.
