del basic_vert.spv
del basic_frag.spv

"../VulkanSDK 1.1.121.2/Bin/glslc.exe" basic.vert -o basic_vert.spv
IF %ERRORLEVEL% NEQ 0 (
   pause
)

"../VulkanSDK 1.1.121.2/Bin/glslc.exe" basic.frag -o basic_frag.spv
IF %ERRORLEVEL% NEQ 0 (
   pause
)