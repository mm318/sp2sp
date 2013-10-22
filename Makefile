##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=sp2sp
IntermediateDirectory  :=./Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
LinkerName             :=gcc
SharedObjectLinkerName :=gcc -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.o.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E 
ObjectsFileList        :="sp2sp.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
RcCmpOptions           := 
RcCompilerName         :=windres
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
IncludePCH             := 
RcIncludePath          := 
Libs                   := 
ArLibs                 :=  
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := ar rcus
CXX      := gcc
CC       := gcc
CXXFLAGS :=  -O2 -Wall $(Preprocessors)
CFLAGS   :=  -O2 -Wall $(Preprocessors)


##
## User defined environment variables
##
CodeLiteDir:=C:\Program Files (x86)\CodeLite
Objects0=$(IntermediateDirectory)/src_sp2sp$(ObjectSuffix) $(IntermediateDirectory)/src_spicestream$(ObjectSuffix) $(IntermediateDirectory)/src_ss_cazm$(ObjectSuffix) $(IntermediateDirectory)/src_ss_hspice$(ObjectSuffix) $(IntermediateDirectory)/src_ss_spice2$(ObjectSuffix) $(IntermediateDirectory)/src_ss_spice3$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

$(IntermediateDirectory)/.d:
	@$(MakeDirCommand) "./Release"

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/src_sp2sp$(ObjectSuffix): src/sp2sp.c $(IntermediateDirectory)/src_sp2sp$(DependSuffix)
	$(CC) $(SourceSwitch) "./src/sp2sp.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_sp2sp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_sp2sp$(DependSuffix): src/sp2sp.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_sp2sp$(ObjectSuffix) -MF$(IntermediateDirectory)/src_sp2sp$(DependSuffix) -MM "src/sp2sp.c"

$(IntermediateDirectory)/src_sp2sp$(PreprocessSuffix): src/sp2sp.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_sp2sp$(PreprocessSuffix) "src/sp2sp.c"

$(IntermediateDirectory)/src_spicestream$(ObjectSuffix): src/spicestream.c $(IntermediateDirectory)/src_spicestream$(DependSuffix)
	$(CC) $(SourceSwitch) "./src/spicestream.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_spicestream$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_spicestream$(DependSuffix): src/spicestream.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_spicestream$(ObjectSuffix) -MF$(IntermediateDirectory)/src_spicestream$(DependSuffix) -MM "src/spicestream.c"

$(IntermediateDirectory)/src_spicestream$(PreprocessSuffix): src/spicestream.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_spicestream$(PreprocessSuffix) "src/spicestream.c"

$(IntermediateDirectory)/src_ss_cazm$(ObjectSuffix): src/ss_cazm.c $(IntermediateDirectory)/src_ss_cazm$(DependSuffix)
	$(CC) $(SourceSwitch) "./src/ss_cazm.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_ss_cazm$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_ss_cazm$(DependSuffix): src/ss_cazm.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_ss_cazm$(ObjectSuffix) -MF$(IntermediateDirectory)/src_ss_cazm$(DependSuffix) -MM "src/ss_cazm.c"

$(IntermediateDirectory)/src_ss_cazm$(PreprocessSuffix): src/ss_cazm.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_ss_cazm$(PreprocessSuffix) "src/ss_cazm.c"

$(IntermediateDirectory)/src_ss_hspice$(ObjectSuffix): src/ss_hspice.c $(IntermediateDirectory)/src_ss_hspice$(DependSuffix)
	$(CC) $(SourceSwitch) "./src/ss_hspice.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_ss_hspice$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_ss_hspice$(DependSuffix): src/ss_hspice.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_ss_hspice$(ObjectSuffix) -MF$(IntermediateDirectory)/src_ss_hspice$(DependSuffix) -MM "src/ss_hspice.c"

$(IntermediateDirectory)/src_ss_hspice$(PreprocessSuffix): src/ss_hspice.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_ss_hspice$(PreprocessSuffix) "src/ss_hspice.c"

$(IntermediateDirectory)/src_ss_spice2$(ObjectSuffix): src/ss_spice2.c $(IntermediateDirectory)/src_ss_spice2$(DependSuffix)
	$(CC) $(SourceSwitch) "./src/ss_spice2.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_ss_spice2$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_ss_spice2$(DependSuffix): src/ss_spice2.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_ss_spice2$(ObjectSuffix) -MF$(IntermediateDirectory)/src_ss_spice2$(DependSuffix) -MM "src/ss_spice2.c"

$(IntermediateDirectory)/src_ss_spice2$(PreprocessSuffix): src/ss_spice2.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_ss_spice2$(PreprocessSuffix) "src/ss_spice2.c"

$(IntermediateDirectory)/src_ss_spice3$(ObjectSuffix): src/ss_spice3.c $(IntermediateDirectory)/src_ss_spice3$(DependSuffix)
	$(CC) $(SourceSwitch) "./src/ss_spice3.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_ss_spice3$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_ss_spice3$(DependSuffix): src/ss_spice3.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_ss_spice3$(ObjectSuffix) -MF$(IntermediateDirectory)/src_ss_spice3$(DependSuffix) -MM "src/ss_spice3.c"

$(IntermediateDirectory)/src_ss_spice3$(PreprocessSuffix): src/ss_spice3.c
	@$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_ss_spice3$(PreprocessSuffix) "src/ss_spice3.c"


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) $(IntermediateDirectory)/src_sp2sp$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/src_sp2sp$(DependSuffix)
	$(RM) $(IntermediateDirectory)/src_sp2sp$(PreprocessSuffix)
	$(RM) $(IntermediateDirectory)/src_spicestream$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/src_spicestream$(DependSuffix)
	$(RM) $(IntermediateDirectory)/src_spicestream$(PreprocessSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_cazm$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_cazm$(DependSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_cazm$(PreprocessSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_hspice$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_hspice$(DependSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_hspice$(PreprocessSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_spice2$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_spice2$(DependSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_spice2$(PreprocessSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_spice3$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_spice3$(DependSuffix)
	$(RM) $(IntermediateDirectory)/src_ss_spice3$(PreprocessSuffix)
	$(RM) $(OutputFile)
	$(RM) $(OutputFile).exe
	$(RM) ".build-release/sp2sp"


