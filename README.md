# ServiceDrawer (svcdrawer)

A small Windows console/service application that wraps and launches another process (e.g. a batch script or `cmd`), captures its output, and logs it to file and to the Windows Event Log. It can run either as a normal command-line tool or as an installed Windows Service.


## Building

Open [svcdrawer.sln](svcdrawer.sln) in Visual Studio and build the `svcdrawer` project (x64, Debug or Release). The compiled binary is placed under `x64/Debug/svcdrawer.exe` (or `x64/Release/...`).

## Usage

### Run interactively

```
svcdrawer.exe <application> [arguments...]
```

Example:

```
svcdrawer.exe cmd /u /c "C:\path\to\runConsole.bat"
```


### Install as a Windows Service

Edit the path in [svcdrawer/register.bat](svcdrawer/register.bat) if needed, then run it (as Administrator):

```
register.bat
```

This registers a service named `ServiceDrawer` (display name "Service Drawer") set to start automatically.

### Uninstall the service

```
delete.bat
```

### Test run

[svcdrawer/testrun.bat](svcdrawer/testrun.bat) launches the built executable against the sample [svcdrawer/test/runConsole.bat](svcdrawer/test/runConsole.bat) script for manual verification.

## Logging

- Console/child-process output is appended to `logs.txt`, which is rotated (deleted and recreated) once it reaches 10MB.
- Service lifecycle and error events are also written to the Windows Event Log under the `ServiceDrawer` source.
