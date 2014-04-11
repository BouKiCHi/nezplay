#ifndef S_MMC5_H__
#define S_MMC5_H__

#ifdef __cplusplus
extern "C" {
#endif

void MMC5SetState(S_STATE *state);
void MMC5SetMask(char *mask);
void MMC5MutiplierInstall(void);
void MMC5SoundInstall(void);
void MMC5ExtendRamInstall(void);

#ifdef __cplusplus
}
#endif

#endif /* S_MMC5_H__ */
