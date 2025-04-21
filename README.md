# cmd
Windows Cmd.exe and Cmd.dll with custom hooks  
The system mimics Windows cmd.exe (from ReactOS) with the following additional features:   

**Command Execution:**  
The executable (cmd.exe) processes command-line arguments and supports options like  
- /C (run and terminate)
- /K (run and remain)
- /I (read from STDIN)
   
**Plugin System:**  
The DLL (cmd.dll) provides a hook-based extensibility mechanism.  
Developers can register custom functions for: 
- Loading files (CMDHOOK_LOADFILE).
- Processing commands (CMDHOOK_COMMAND).
- Executing commands (CMDHOOK_EXECUTE).
   
The cmd_sethook function allows dynamic registration of these hooks at runtime.  

**Custom Commands:**   
   The cmd_custom function in main.c demonstrates how to handle custom commands (sayhello and perlcall).  
   Unknown commands return -1, allowing the system to fall back to default behavior.  
  

**Initialization:**   
Both the DLL and executable initialize by storing the full path and directory of the module/executable.   
This information can be used for file operations or resolving relative paths.   

**Strengths**
- Modularity : The hook system allows adding functionality without modifying the DLL, making it extensible. 
- Lightweight : The code is minimal, focusing on core command-line functionality. 
- Clear Interface : cmdhooks.h provides a well-defined API for plugins. 
- Customizability : Developers can easily add new commands or override default behavior via hooks. 
  
  
**Building**  
The 32 and 64 bit executables were build with Visual Studio 2008 C++ and have no dependencies.
  
