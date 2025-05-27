#ifndef DIAGTEST_H_
#define DIAGTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Add your declarations here */
extern int proc_testSpeaker(char *buf);
extern int proc_setVolume(char *buf);
extern int proc_testMIC();
extern int play_sin(int freq, int volume, uint16_t msec);
extern int play_setVolume(int vol);
extern int play_getVolume(void);
extern int play_Mute(int on);
extern int send_message(char *message);
extern int proc_outputSource();
#ifdef __cplusplus
}
#endif

#endif /* DIAGTEST_H_ */