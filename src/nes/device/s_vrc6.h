#ifndef S_VRC6_H__
#define S_VRC6_H__

#ifdef __cplusplus
extern "C" {
#endif

void VRC6SetState(S_STATE *state);
void VRC6SetMask(char *mask);
void VRC6SoundInstall(void);

#ifdef __cplusplus
}
#endif

#endif /* S_VRC6_H__ */
