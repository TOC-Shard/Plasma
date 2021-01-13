include(FindPackageHandleStandardArgs)

find_path(3dsm_PATH include/maxapi.h)

find_path(3dsm_INCLUDE_DIR maxapi.h
    ${3dsm_PATH}/include
)

find_library(3dsm_BMM_LIBRARY bmm
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_CORE_LIBRARY core
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_CUSTDLG_LIBRARY CustDlg
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_GEOM_LIBRARY geom
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_GFX_LIBRARY gfx
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_GUP_LIBRARY gup
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_MANIPSYS_LIBRARY manipsys
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_MAXSCRPT_LIBRARY Maxscrpt
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_MAXUTIL_LIBRARY maxutil
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_MESH_LIBRARY mesh
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_MENUS_LIBRARY menus
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_MNMATH_LIBRARY mnmath
    PATHS "${3dsm_PATH}/lib"
)
find_library(3dsm_PARAMBLK2_LIBRARY paramblk2
    PATHS "${3dsm_PATH}/lib"
)

find_package_handle_standard_args(
    3dsm
    REQUIRED_VARS 3dsm_BMM_LIBRARY
                  3dsm_CORE_LIBRARY
                  3dsm_CUSTDLG_LIBRARY
                  3dsm_GEOM_LIBRARY
                  3dsm_GFX_LIBRARY
                  3dsm_GUP_LIBRARY
                  3dsm_MANIPSYS_LIBRARY
                  3dsm_MAXSCRPT_LIBRARY
                  3dsm_MAXUTIL_LIBRARY
                  3dsm_MESH_LIBRARY
                  3dsm_MENUS_LIBRARY
                  3dsm_MNMATH_LIBRARY
                  3dsm_PARAMBLK2_LIBRARY
)

set(3dsm_INCLUDE_DIRS ${3dsm_INCLUDE_DIR})
set(3dsm_LIBRARIES
    ${3dsm_BMM_LIBRARY}
    ${3dsm_CORE_LIBRARY}
    ${3dsm_CUSTDLG_LIBRARY}
    ${3dsm_GEOM_LIBRARY}
    ${3dsm_GFX_LIBRARY}
    ${3dsm_GUP_LIBRARY}
    ${3dsm_MANIPSYS_LIBRARY}
    ${3dsm_MAXSCRPT_LIBRARY}
    ${3dsm_MAXUTIL_LIBRARY}
    ${3dsm_MESH_LIBRARY}
    ${3dsm_MENUS_LIBRARY}
    ${3dsm_MNMATH_LIBRARY}
    ${3dsm_PARAMBLK2_LIBRARY}
)

mark_as_advanced(
    3dsm_BMM_LIBRARY
    3dsm_CORE_LIBRARY
    3dsm_CUSTDLG_LIBRARY
    3dsm_GEOM_LIBRARY
    3dsm_GFX_LIBRARY
    3dsm_GUP_LIBRARY
    3dsm_MANIPSYS_LIBRARY
    3dsm_MAXSCRPT_LIBRARY
    3dsm_MAXUTIL_LIBRARY
    3dsm_MESH_LIBRARY
    3dsm_MENUS_LIBRARY
    3dsm_MNMATH_LIBRARY
    3dsm_PARAMBLK2_LIBRARY
)
