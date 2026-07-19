#pragma once

// XPT2046 resistive touch, CYD wiring. Compiled in only with -DFEATURE_TOUCH=1
// so the firmware still builds and runs on touchless boards.
#if FEATURE_TOUCH
void touchInit();
void touchApplyRotation();
int touchTap();  // -1 none, 0 left half, 1 right half
#else
inline void touchInit() {}
inline void touchApplyRotation() {}
inline int touchTap() { return -1; }
#endif
