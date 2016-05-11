#include <QtCore/QLibrary>

#include <pro.h>

#include <llvm/IR/Module.h>

#include "idallvm/msg.h"
#include "idallvm/libqemu.h"

static QLibrary* lib = nullptr;

decltype(libqemu_init)* Libqemu_Init = nullptr;
decltype(libqemu_raise_error)* Libqemu_RaiseError = nullptr;
decltype(libqemu_gen_intermediate_code)* Libqemu_GenIntermediateCode = nullptr;
decltype(libqemu_get_module)* Libqemu_GetModule = nullptr;
decltype(libqemu_get_target_name)* Libqemu_GetTargetName = nullptr;
decltype(libqemu_get_register_info_by_name)* Libqemu_GetRegisterInfoByName = nullptr;
decltype(libqemu_get_register_info_by_offset)* Libqemu_GetRegisterInfoByOffset = nullptr;
decltype(libqemu_get_register_info_by_indices)* Libqemu_GetRegisterInfoByIndices = nullptr;
decltype(libqemu_get_register_info_pc)* Libqemu_GetRegisterInfoPc = nullptr;
decltype(libqemu_get_register_info_sp)* Libqemu_GetRegisterInfoSp = nullptr;

static int is_searched_plugin(const char* file, void* ud)
{
    return 1;
}

static int get_plugin_dir(char* path, size_t path_size)
{
    const extlang_t* extlang = NULL;
    int err = enum_plugins(is_searched_plugin,
            NULL,
            path,
            path_size,
            &extlang);
    if (*path) {
        qdirname(path, path_size, path);
    }

    return 0;
}


llvm::Type* Libqemu_GetCpustateType(void) 
{
    llvm::Module* module = llvm::unwrap(Libqemu_GetModule());
    assert(module && "Cannot get libqemu LLVM module");
    llvm::GlobalVariable* cpu_type_anchor = module->getGlobalVariable("cpu_type_anchor", false);
    assert(cpu_type_anchor && "Cannot get cpu_type_anchor variable in LLVM module");
    llvm::PointerType* ptr_type = llvm::cast<llvm::PointerType>(cpu_type_anchor->getType());
    assert(ptr_type && "Cannot get pointer type of cpu_type_anchor variable");
    llvm::Type* type = ptr_type->getElementType();
    assert(type && "Cannot get type of cpu_type_anchor variable");
    return type;
}

int Libqemu_Load(Processor processor)
{
    const char* libname = NULL;
    char plugins_dir[256];
    switch(processor) {
        case PROCESSOR_ARM:
            libname = LIBQEMU_LIBNAME_ARM;
            break;
    }

    get_plugin_dir(plugins_dir, sizeof(plugins_dir));
    qmakepath(plugins_dir, sizeof(plugins_dir), plugins_dir, libname);

    lib = new QLibrary(plugins_dir);
    lib->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!lib->load()) {
        MSG_ERROR("Cannot open libqemu library %s: %s", plugins_dir, lib->errorString().toLatin1().data());
        return -1;
    }

#define LOAD_FUNCTION_POINTER(var, name) \
    var = reinterpret_cast<decltype(var)>(lib->resolve(name)); \
    if (!var) { \
        MSG_ERROR("Cannot get function pointer for %s", name); \
        lib->unload(); \
        return -1; \
    }

    LOAD_FUNCTION_POINTER(Libqemu_Init, "libqemu_init")
    LOAD_FUNCTION_POINTER(Libqemu_GenIntermediateCode, "libqemu_gen_intermediate_code")
    LOAD_FUNCTION_POINTER(Libqemu_RaiseError, "libqemu_raise_error")
    LOAD_FUNCTION_POINTER(Libqemu_GetTargetName, "libqemu_get_target_name")
    LOAD_FUNCTION_POINTER(Libqemu_GetModule, "libqemu_get_module")
    LOAD_FUNCTION_POINTER(Libqemu_GetRegisterInfoByName, "libqemu_get_register_info_by_name");
    LOAD_FUNCTION_POINTER(Libqemu_GetRegisterInfoByOffset, "libqemu_get_register_info_by_offset");
    LOAD_FUNCTION_POINTER(Libqemu_GetRegisterInfoByIndices, "libqemu_get_register_info_by_indices");
    LOAD_FUNCTION_POINTER(Libqemu_GetRegisterInfoPc, "libqemu_get_register_info_pc");
    LOAD_FUNCTION_POINTER(Libqemu_GetRegisterInfoSp, "libqemu_get_register_info_sp");

#undef LOAD_FUNCTION_POINTER
    return 0;
}

void Libqemu_Unload(void)
{
    if (lib) {
        lib->unload();
        lib = NULL;
    }

    Libqemu_Init = nullptr;
    Libqemu_RaiseError = nullptr;
    Libqemu_GenIntermediateCode = nullptr;
    Libqemu_GetTargetName = nullptr;
    Libqemu_GetModule = nullptr;
    Libqemu_GetRegisterInfoByName = nullptr;
    Libqemu_GetRegisterInfoByOffset = nullptr;
    Libqemu_GetRegisterInfoByIndices = nullptr;
    Libqemu_GetRegisterInfoPc = nullptr;
    Libqemu_GetRegisterInfoSp = nullptr;
}

