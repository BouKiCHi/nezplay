#ifndef S_APU_H__
#define S_APU_H__

#ifdef __cplusplus
extern "C" {
#endif

void APUSetState(S_STATE *state);
void APUSoundInstall(void);
void APUSetMask(char *mask);

#ifdef __cplusplus
}
#endif

#endif /* S_APU_H__ */
