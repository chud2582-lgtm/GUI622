/*
 * Copyright (C) 2017 by
 * Shenzhen Inovance Technology Co., Ltd.
 */
#ifndef IMC100API_H
#define IMC100API_H

#ifdef IMC100API_EXPORTS
#define IMC100API __declspec(dllexport)
#else
#define IMC100API __declspec(dllimport)
#endif

#define FUNTYPE __stdcall


typedef struct 
{
    double RPosData[6];				/* robot pos */
	int ArmParm[4];					/* arm parameters */
    double EPosData[6];				/* ext joint pos */
}ROB_POS;

typedef struct 
{
    double JointData[8];			/* robot joint pos(reserve joint7 & joint 8) */
    double EPosData[6];				/* ext joint pos */
}ROB_JPOS;

typedef struct 
{
    double Data[6];
}POSE;

typedef struct 
{
    double Mass;
	double Cog[3];
	double Orient[3];
	double Inertia[3];
}LOAD_DATA;

typedef struct
{
	int RobHold;
	POSE TFrame;
	LOAD_DATA TLoad;
}TOOL_DATA;

typedef struct 
{
    int RobHold;
	int UFFix;
	char UFMec[18];
	POSE UFrame;
	POSE OFrame;
}WOBJ_DATA;

typedef struct 
{
	double vTcp;
	double vOri;
	double vLeax;
	double rReax;
	int bStatic;
}SPEED;

typedef struct 
{
	int velType;   /* 0-VelPercent mode; 1-VelSpeed mode */
	int velPercent;
	SPEED speed;
}VER_DATA;

typedef struct 
{
    int IONo;                         /// DO number
    int IOVa;                         /// output value
    int Kind;                         /// set type
    double Value;                     /// set value
}MOV_IO;

#include <stdint.h>
#pragma pack(push)
#pragma pack(1)

/* 干涉区 */
typedef struct S_INTERFER_ZONE
{
    int32_t Input;           /* 输入信号 */
    int32_t Output;          /* 输出信号 */
    uint16_t Scope;          /* 内外侧 0-内侧, 1-外侧 */
    uint16_t IsAlert;        /* 是否报警 0-无, 1-报警 */
    float SafeL;             /* 安全距离 */
    uint16_t WobjNum;        /* 当前工件号 */

    /* 干涉区 */
    uint16_t SetType;        /* 设置方式0-对角, 1-基准点+边长 */
    float Diagonal[6];       /* 对角点 */
    float PointL[6];         /* 基准点+偏移 */
}S_INTERFER_ZONE;

/* 多TCP */
typedef struct S_INTERFER_TCP_BOX
{
    uint16_t IsUse[4];          /* 是否配置 */
    uint16_t ToolNum[4];        /* 工具号 */
}S_INTERFER_TCP_BOX;

/* 球型包围盒 */
typedef struct S_INTERFER_BALL_BOX
{
    float ZPos;                       /* 偏移点位 */
    float BallR;                        /* 球形半径 */
}S_INTERFER_BALL_BOX;

/* 方形包围盒 */
typedef struct S_INTERFER_SQUARE_BOX
{
    uint16_t SetType;    /* 设置方式0-对角，1-基准点+边长，2-取点 */
    float Diagonal[6];   /* 对角点 */
    float PointL[6];     /* 基准点+偏移 */
    float PointH[13];    /* 取点+高度 */
}S_INTERFER_SQUARE_BOX;

/* 监控对象 */
typedef struct S_INTERFER_TOOL
{
    uint16_t Type;          /* 监控对象类型 0-TCP 1-MTCP 2-BALL 3-SQUARE */
    S_INTERFER_TCP_BOX MTcpBox;         /* 多TCP */
    S_INTERFER_BALL_BOX BallBox;        /* 球形盒 */
    S_INTERFER_SQUARE_BOX SquareBox;    /* 方形盒 */
}S_INTERFER_TOOL;
#pragma pack(pop)


#ifdef __cplusplus 
extern "C" {
#endif

IMC100API int FUNTYPE IMC100_Init_ETH(unsigned int ipAddr, unsigned short ipPort, int timeOut, int comId);
IMC100API int FUNTYPE IMC100_Exit_ETH(int comId);

IMC100API int FUNTYPE IMC100_EmergStop(int cmd, int comId);
IMC100API int FUNTYPE IMC100_MotorEnable(int cmd, int comId);
IMC100API int FUNTYPE IMC100_ResetErr(int comId);
IMC100API int FUNTYPE IMC100_Set_Mode(int mode, int comId);
IMC100API int FUNTYPE IMC100_PrgCtrl(int cmd, int comId);
IMC100API int FUNTYPE IMC100_BackStartLine(int comId);
IMC100API int FUNTYPE IMC100_Set_Vel(int val, int comId);
IMC100API int FUNTYPE IMC100_Set_AccRamp(double startVal, double endVal , int comId);
IMC100API int FUNTYPE IMC100_Set_RapidMove(int movType, int enableFlag, int comId);
IMC100API int FUNTYPE IMC100_Set_SLVSMode(int mode, int comId);
IMC100API int FUNTYPE IMC100_Set_FlyMode(int cpMode, int flyMode, int comId);
IMC100API int FUNTYPE IMC100_Set_FlyPress(int flyPressPos, int flyPressOrient, int comId);
IMC100API int FUNTYPE IMC100_DsMode(int cmd, int comId);
IMC100API int FUNTYPE IMC100_Set_SlewMode(int cmd, int comId);
IMC100API int FUNTYPE IMC100_Set_DO(int num, int status, int comId);
IMC100API int FUNTYPE IMC100_Set_DOGroup(int num, int status, int comId);
IMC100API int FUNTYPE IMC100_Set_DA(int num, float val, int comId);
IMC100API int FUNTYPE IMC100_InchMode(int cmd, int comId);
IMC100API int FUNTYPE IMC100_Set_InchStep(int val, int comId);


IMC100API int FUNTYPE IMC100_AxisJog(int axis, int cmd, int comId);
IMC100API int FUNTYPE IMC100_AxisInch(int axis, int cmd, int comId);
IMC100API int FUNTYPE IMC100_PoseAlign(int coord, int cmd, int comId);
IMC100API int FUNTYPE IMC100_Set_ActiveMechUnit(char *mecUnit, int comId);
IMC100API int FUNTYPE IMC100_Get_ActiveMechUnit(char *mecUnit, int comId);
IMC100API int FUNTYPE IMC100_Set_TeachCoordinate(int flag, int comId);
IMC100API int FUNTYPE IMC100_Get_TeachCoordinate(int *flag, int comId);
IMC100API int FUNTYPE IMC100_Home(int num, int comId);
IMC100API int FUNTYPE IMC100_Set_DynamicBrake(int flag, int comId);
IMC100API int FUNTYPE IMC100_Get_DynamicBrake(int *flag, int comId);
IMC100API int FUNTYPE IMC100_MovJ_P(int posNum, int vel, int zone, int comId);
IMC100API int FUNTYPE IMC100_MovL_P(int posNum, int vel, int zone, int comId);
IMC100API int FUNTYPE IMC100_MovC_P(int posMidNum, int posDstNum, int vel, int zone, int comId);

IMC100API int FUNTYPE IMC100_MovJ_P_IO(int posNum, int vel, int zone, MOV_IO *movIo, int ioNum, int comId);
IMC100API int FUNTYPE IMC100_MovL_P_IO(int posNum, int vel, int zone, MOV_IO *movIo, int ioNum, int comId);
IMC100API int FUNTYPE IMC100_MovC_P_IO(int posMidNum, int posDstNum, int vel, int zone, MOV_IO *movIo, int ioNum, int comId);

IMC100API int FUNTYPE IMC100_Jump_P(int posNum, int vel, int zone, int comId);
IMC100API int FUNTYPE IMC100_JumpL_P(int posNum, int vel, int zone, int comId);


IMC100API int FUNTYPE IMC100_Jump_P_IO(int posNum, int vel, int zone, MOV_IO *movIo, int ioNum, int comId);
IMC100API int FUNTYPE IMC100_JumpL_P_IO(int posNum, int vel, int zone, MOV_IO *movIo, int ioNum, int comId);

IMC100API int FUNTYPE IMC100_MovJ_RobPos(ROB_POS *pos,VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);
IMC100API int FUNTYPE IMC100_MovL_RobPos(ROB_POS *pos,VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);
IMC100API int FUNTYPE IMC100_MovC_RobPos(ROB_POS *posMid,ROB_POS *posDst, VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);
IMC100API int FUNTYPE IMC100_Jump_RobPos(ROB_POS *pos, VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);
IMC100API int FUNTYPE IMC100_JumpL_RobPos(ROB_POS *pos, VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);

IMC100API int FUNTYPE IMC100_MovJAbs_RobJPos(ROB_JPOS *pos, VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);
IMC100API int FUNTYPE IMC100_MovJAbs_JP(int posNum, VER_DATA *vel, int zone, int ioNum, MOV_IO *movIo, int comId);

IMC100API int FUNTYPE IMC100_IndCMove(char *mecUnit, int axis, double speed, double acc,double dec, int comId);
IMC100API int FUNTYPE IMC100_IndSpeed(char *mecUnit, int axis, int *sts, int comId);
IMC100API int FUNTYPE IMC100_IndResetOld(char *mecUnit, int axis, int comId);
IMC100API int FUNTYPE IMC100_IndReset(char *mecUnit, int axis, double refNum, int direction, int comId);
IMC100API int FUNTYPE IMC100_Get_IndCMoveSts(char *mecUnit, int axis,  int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_IndResetSts(char *mecUnit, int axis,  int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_MotionStsExceptIndAxis(int *sts, int comId);

IMC100API int FUNTYPE IMC100_Get_RobPosHere(ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Get_RobJPosHere(ROB_JPOS *pos, int comId);
IMC100API int FUNTYPE IMC100_Get_PosHerePulse(double pos[6], int comId);
IMC100API int FUNTYPE IMC100_Set_ActiveMechUnitPosFormat(int posFormat, int comId);
IMC100API int FUNTYPE IMC100_Get_ActiveMechUnitPos(int *posFormat, double *mechUnitPos, int comId);

IMC100API int FUNTYPE IMC100_Get_RobJToRobP(ROB_JPOS *posSrc, int toolnum, int wobjnum, int loadnum, ROB_POS *posDst, int comId);
IMC100API int FUNTYPE IMC100_Get_RobPToRobJ(ROB_POS *posSrc, int toolnum, int wobjnum, int loadnum, ROB_JPOS *posDst, int comId);
IMC100API int FUNTYPE IMC100_Get_FixWobjRobP(ROB_POS *posBase, int wobjnum, ROB_POS *posDst, int comId);
IMC100API int FUNTYPE IMC100_Get_RobHoldWobjRobP(ROB_POS *posBase, int wobjnum, ROB_POS *posRef, ROB_POS *posDst, int comId);

IMC100API int FUNTYPE IMC100_Get_OffsetJ_RobJP(ROB_JPOS *posSrc, double PR[6], ROB_JPOS *posDst, int comId);
IMC100API int FUNTYPE IMC100_Get_Offset_RobP(ROB_POS *posSrc, double PR[6], ROB_POS *posDst, int comId);
IMC100API int FUNTYPE IMC100_Get_OffsetT_RobP(ROB_POS *posSrc, double PR[6], ROB_POS *posDst, int comId);

IMC100API int FUNTYPE IMC100_Get_SysErrSts(int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_SysErr(int *error, int comId);

IMC100API int FUNTYPE IMC100_Get_TaskPrgPath(int taskId, char prgPath[128], int comId);
IMC100API int FUNTYPE IMC100_Get_TaskRunSts(int taskId, int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_TaskProgramLine(int taskId, int *line, int comId);
IMC100API int FUNTYPE IMC100_Get_CurMotionLine(int *line, int comId);
IMC100API int FUNTYPE IMC100_Get_InitSts(int *sts, int comId);

IMC100API int FUNTYPE IMC100_Get_CoordType(int *type, int comId);
IMC100API int FUNTYPE IMC100_Get_AccRamp(double *startVal, double *endVal, int comId);
IMC100API int FUNTYPE IMC100_Get_RapidMove(int movType, int *enableFlag, int comId);
IMC100API int FUNTYPE IMC100_Get_SLVSMode(int *mode, int comId);
IMC100API int FUNTYPE IMC100_Get_FlyMode(int cpMode, int *flyMode, int comId);
IMC100API int FUNTYPE IMC100_Get_FlyPress(int *flyPressPos, int *flyPressOrient, int comId);
IMC100API int FUNTYPE IMC100_Get_Vel(int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_Mode(int *mode, int comId);
IMC100API int FUNTYPE IMC100_Get_DsMode(int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_InchMode(int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_SlewMode(int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_EStopSts(int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_MotorSts(int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_MotionSts(int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_SysMode(int *mode, int comId);
IMC100API int FUNTYPE IMC100_Get_PrgRunTime(unsigned int *second, int comId);
IMC100API int FUNTYPE IMC100_Get_CurCmdNum(unsigned int *num, int comId);
IMC100API int FUNTYPE IMC100_Get_CurCmdSts(int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_CmdSts(int num, int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_CurCmdCacheNum(int *num, int comId);

IMC100API int FUNTYPE IMC100_Get_DINum(int *num, int comId);
IMC100API int FUNTYPE IMC100_Get_DONum(int *num, int comId);
IMC100API int FUNTYPE IMC100_Get_ADNum(int *num, int comId);
IMC100API int FUNTYPE IMC100_Get_DANum(int *num, int comId);
IMC100API int FUNTYPE IMC100_Get_DI(int num, int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_DIGroup(int num, int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_AD(int num, float *val, int comId);
IMC100API int FUNTYPE IMC100_Get_DOCfg(int num, int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_DOGroupCfg(int num, int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_DO(int num, int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_DOGroup(int num, int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_DACfg(int num, int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_DA(int num, float *val, int comId);
IMC100API int FUNTYPE IMC100_Get_DevSts(int sts[6], int comId);
IMC100API int FUNTYPE IMC100_Get_FwVersion(char ver[32], int comId);
IMC100API int FUNTYPE IMC100_Get_SysTime(char time[16], int comId);
IMC100API int FUNTYPE IMC100_Get_RobotType(char type[128], int comId);
IMC100API int FUNTYPE IMC100_Get_ArmType(double pos[6], int armType[4], int comId);

IMC100API int FUNTYPE IMC100_Get_ServoSts(int sts[8], int comId);
IMC100API int FUNTYPE IMC100_Get_ServoErr(int num, int *error, int comId);

IMC100API int FUNTYPE IMC100_Get_StrPara(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_StrPara(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_StrParaComp(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_StrParaComp(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_SupplementaryStrParamComp(float para[14], int comId);
IMC100API int FUNTYPE IMC100_Set_SupplementaryStrParamComp(float para[14], int comId);
IMC100API int FUNTYPE IMC100_Get_RdctRatio(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_RdctRatio(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_CpParaM(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_CpParaM(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_CpParaS(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_CpParaS(float para[6], int comId);

IMC100API int FUNTYPE IMC100_Get_HomeJPos(int num, ROB_JPOS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_HomeJPos(int num, ROB_JPOS *pos, int comId);
IMC100API int FUNTYPE IMC100_Get_ZeroPos(int pluse[6], int comId);
IMC100API int FUNTYPE IMC100_Set_ZeroPos(int pluse[6], int comId);
IMC100API int FUNTYPE IMC100_Get_InchStep(int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_StepMotionJ(float *para, int comId);
IMC100API int FUNTYPE IMC100_Set_StepMotionJ(float para, int comId);
IMC100API int FUNTYPE IMC100_Get_StepMotionL(float *para, int comId);
IMC100API int FUNTYPE IMC100_Set_StepMotionL(float para, int comId);
IMC100API int FUNTYPE IMC100_Get_StepMotionR(float *para, int comId);
IMC100API int FUNTYPE IMC100_Set_StepMotionR(float para, int comId);
IMC100API int FUNTYPE IMC100_Get_TeachVelLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_TeachVelLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_TeachVelLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Set_TeachVelLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Get_TeachAccLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_TeachAccLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_TeachAccLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Set_TeachAccLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Get_RunVelLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_RunVelLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_RunVelLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Set_RunVelLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Get_RunAccLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_RunAccLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_RunAccLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Set_RunAccLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Get_StopDecLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Set_StopDecLimJ(float para[6], int comId);
IMC100API int FUNTYPE IMC100_Get_StopDecLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Set_StopDecLimL(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Get_ZonePara(float para[2], int comId);
IMC100API int FUNTYPE IMC100_Set_ZonePara(float para[2], int comId);

IMC100API int FUNTYPE IMC100_Get_AxisNLim(int axis, float *para, int comId);
IMC100API int FUNTYPE IMC100_Set_AxisNLim(int axis, float para, int comId);
IMC100API int FUNTYPE IMC100_Get_AxisPLim(int axis, float *para, int comId);
IMC100API int FUNTYPE IMC100_Set_AxisPLim(int axis, float para, int comId);

IMC100API int FUNTYPE IMC100_Get_ToolData(int num, TOOL_DATA *para, int comId);
IMC100API int FUNTYPE IMC100_Set_ToolData(int num, TOOL_DATA *para, int comId);
IMC100API int FUNTYPE IMC100_Get_WobjData(int num, WOBJ_DATA *para, int comId);
IMC100API int FUNTYPE IMC100_Set_WobjData(int num, WOBJ_DATA *para, int comId);

IMC100API int FUNTYPE IMC100_Get_ToolCNum(int *num, int comId);
IMC100API int FUNTYPE IMC100_Set_ToolCNum(int num, int comId);
IMC100API int FUNTYPE IMC100_Get_WobjNum(int *num, int comId);
IMC100API int FUNTYPE IMC100_Set_WobjNum(int num, int comId);
IMC100API int FUNTYPE IMC100_Set_CoordType(int type, int comId);

IMC100API int FUNTYPE IMC100_Get_JumpPara(float *lh, float *mh, float *rh, int comId);
IMC100API int FUNTYPE IMC100_Set_JumpPara(float lh, float mh, float rh, int comId);
IMC100API int FUNTYPE IMC100_Get_PalletPara(int *rowNum, int *colNum,  int *layerNum, double *layerHeight, int comId);
IMC100API int FUNTYPE IMC100_Set_PalletPara(int rowNum, int colNum, int layerNum, double layerHeight, int comId);
IMC100API int FUNTYPE IMC100_Clear_PalletPara(int comId);
IMC100API int FUNTYPE IMC100_Get_Pallet_RobP(ROB_POS *pos1, ROB_POS *pos2, ROB_POS *pos3, int rowIndex, int colIndex, int layIndex, ROB_POS *posDst, int comId);
IMC100API int FUNTYPE IMC100_Get_Pallet4_RobP(ROB_POS *pos1, ROB_POS *pos2, ROB_POS *pos3, ROB_POS *pos4, int rowIndex, int colIndex, int layIndex, ROB_POS *posDst, int comId);

IMC100API int FUNTYPE IMC100_SavePara(int comId);
IMC100API int FUNTYPE IMC100_RecoverPara(int comId);

IMC100API int FUNTYPE IMC100_Get_RobP(int pNum, ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_RobP(int pNum, ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_MemRobP(int pNum, ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Get_RobPFromFile(char *pFileName, int pNum, ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_RobPToFile(char *pFileName, int pNum, ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Get_CurRobPFileName(char *pFileName, int comId);
IMC100API int FUNTYPE IMC100_Set_RobPHere(int pNum, int comId);
IMC100API int FUNTYPE IMC100_Set_RobPHereToFile(char *pFileName, int pNum, int comId);
IMC100API int FUNTYPE IMC100_Get_RobJP(int pNum, ROB_JPOS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_RobJP(int pNum, ROB_JPOS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_MemRobJP(int pNum, ROB_JPOS *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_RobJPHere(int pNum, int comId);
IMC100API int FUNTYPE IMC100_Get_RobP_Label(int pNum, char *plabelName, int comId);
IMC100API int FUNTYPE IMC100_Get_RobJP_Label(int pNum, char *plabelName, int comId);
IMC100API int FUNTYPE IMC100_Get_RobLP_Label(int taskId, char *pProName, int pNum, char *plabelName, int comId);

IMC100API int FUNTYPE IMC100_Get_PRVar(int prNum, POSE *pos, int comId);
IMC100API int FUNTYPE IMC100_Set_PRVar(int prNum, POSE pos, int comId);

IMC100API int FUNTYPE IMC100_Get_B(int num, int *val, int comId);
IMC100API int FUNTYPE IMC100_Set_B(int num, int val, int comId);
IMC100API int FUNTYPE IMC100_Get_R(int num, int *val, int comId);
IMC100API int FUNTYPE IMC100_Set_R(int num, int val, int comId);
IMC100API int FUNTYPE IMC100_Get_D(int num, double *val, int comId);
IMC100API int FUNTYPE IMC100_Set_D(int num, double val, int comId);

IMC100API int FUNTYPE IMC100_Get_ModbusCoil(int address, int sum, int *val, int comId);
IMC100API int FUNTYPE IMC100_Set_ModbusCoil(int address, int sum, int val, int comId);
IMC100API int FUNTYPE IMC100_Get_ModbusRegUshort(int address, int sum, unsigned short val[], int comId);
IMC100API int FUNTYPE IMC100_Set_ModbusRegUshort(int address, int sum, unsigned short val[], int comId);
IMC100API int FUNTYPE IMC100_Get_ModbusRegFloat(int address, int sum, float val[], int comId);
IMC100API int FUNTYPE IMC100_Set_ModbusRegFloat(int address, int sum, float val[], int comId);
IMC100API int FUNTYPE IMC100_Get_PlcVarByte(int num, unsigned char *val, int comId);
IMC100API int FUNTYPE IMC100_Get_PlcVarInt(int num, short *val, int comId);
IMC100API int FUNTYPE IMC100_Get_PlcVarDInt(int num, int *val, int comId);
IMC100API int FUNTYPE IMC100_Get_PlcVarLReal(int num, double *val, int comId);
IMC100API int FUNTYPE IMC100_Get_UserAlarm(int num, char alarm[40], int comId);
IMC100API int FUNTYPE IMC100_Set_UserAlarm(int num, char alarm[40], int comId);
IMC100API int FUNTYPE IMC100_Get_Print(char val[128], int comId);

IMC100API int FUNTYPE IMC100_CurCtrlDev(int *dev, int comId);
IMC100API int FUNTYPE IMC100_CurPermit(int *owner, unsigned int *ipAddr, unsigned short *ipPort, int comId);
IMC100API int FUNTYPE IMC100_AcqPermit(int cmd, int comId);
IMC100API int FUNTYPE IMC100_RemovePermit(int comId);
IMC100API int FUNTYPE IMC100_CurUserType(int *type, int comId);
IMC100API int FUNTYPE IMC100_UserLogin(int type, char password[8], int comId);
IMC100API int FUNTYPE IMC100_UserLogout(int comId);

IMC100API int FUNTYPE IMC100_Set_SysTime(char time[16], int comId);

IMC100API int FUNTYPE IMC100_LatchEnable(int cmd, int comId);
IMC100API int FUNTYPE IMC100_Get_LatchSts(int *sts, int comId);
IMC100API int FUNTYPE IMC100_Get_LatchSum(int *sum, int comId);

IMC100API int FUNTYPE IMC100_Get_LatchRobP(int index, int *sts, ROB_POS *pos, int comId);
IMC100API int FUNTYPE IMC100_Clr_LatchPos(int comId);

IMC100API int FUNTYPE IMC100_Set_CollModeAndAction(int checkflag, int action, int comId);
IMC100API int FUNTYPE IMC100_Get_CollModeAndAction(int *checkflag, int *action, int comId);
IMC100API int FUNTYPE IMC100_Set_AxisCollMode(int axisNo, int checkflag, int comId);
IMC100API int FUNTYPE IMC100_Get_AxisCollMode(int axisNo, int *checkflag, int comId);
IMC100API int FUNTYPE IMC100_Set_AxisCollLevel(int axisNo, double level, int comId);
IMC100API int FUNTYPE IMC100_Get_AxisCollLevel(int axisNo, double *level, int comId);
IMC100API int FUNTYPE IMC100_Set_CollDist(int dist, int comId);
IMC100API int FUNTYPE IMC100_Get_CollDist(int *dist, int comId);

IMC100API int FUNTYPE IMC100_Set_TeachModeCollAction(int action, int comId);
IMC100API int FUNTYPE IMC100_Get_TeachModeCollAction(int *action, int comId);
IMC100API int FUNTYPE IMC100_Set_TeachModeAxisCollLevel(int axisNo, double level, int comId);
IMC100API int FUNTYPE IMC100_Get_TeachModeAxisCollLevel(int axisNo, double *level, int comId);
IMC100API int FUNTYPE IMC100_Set_TeachModeCollDist(int dist, int comId);
IMC100API int FUNTYPE IMC100_Get_TeachModeCollDist(int *dist, int comId);

IMC100API int FUNTYPE IMC100_Set_PlayBackModeCollAction(int action, int comId);
IMC100API int FUNTYPE IMC100_Get_PlayBackModeCollAction(int *action, int comId);
IMC100API int FUNTYPE IMC100_Set_PlayBackModeAxisCollLevel(int axisNo, double level, int comId);
IMC100API int FUNTYPE IMC100_Get_PlayBackModeAxisCollLevel(int axisNo, double *level, int comId);
IMC100API int FUNTYPE IMC100_Set_PlayBackModeCollDist(int dist, int comId);
IMC100API int FUNTYPE IMC100_Get_PlayBackModeCollDist(int *dist, int comId);

IMC100API int FUNTYPE IMC100_Get_RobotAxisNum(int *axisNum, int comId);

IMC100API int FUNTYPE IMC100_Set_BindTcpSpeedValue(int num, double minValue, double maxValue, double minSpeed, double maxSpeed, int comId);
IMC100API int FUNTYPE IMC100_Get_BindTcpSpeedValue(int num, int *pStatus, double *pMinValue, double *pMaxValue, double *pMinSpeed, double *pMaxSpeed, int comId);
IMC100API int FUNTYPE IMC100_Get_TcpSpeedDAOutAndVel(int num, int *pStatus, double *pDAout, double *pVelocity, int comId);
IMC100API int FUNTYPE IMC100_Close_BindTcpSpeed(int num, int comId);

IMC100API int FUNTYPE IMC100_Set_TraceRecoverMode(int TraceMode, int comId);
IMC100API int FUNTYPE IMC100_Get_TraceRecoverMode(int *TraceMode,int comId);
IMC100API int FUNTYPE IMC100_Set_VarTraceRecoverParams(int Mode,double TCPDis, double TCPRot, double ExternalDis, double ExternalRot, int comId);
IMC100API int FUNTYPE IMC100_Get_VarTraceRecoverParams(int Mode, double *TCPDistance, double *TCPRotation, double *ExternalDistance, double *ExternalRotation,int comId);

IMC100API int FUNTYPE IMC100_Set_InterferZoneActStat(int iNum, int iStatus, int comId);
IMC100API int FUNTYPE IMC100_Get_InterferZoneActStat(int iStatus[16], int comId);
IMC100API int FUNTYPE IMC100_Set_InterferZonePara(int iNum, S_INTERFER_ZONE* pstPara, int comId);
IMC100API int FUNTYPE IMC100_Get_InterferZonePara(int iNum, S_INTERFER_ZONE* pstPara, int comId);

IMC100API int FUNTYPE IMC100_Set_InterferToolActNum(int iNum, int comId);
IMC100API int FUNTYPE IMC100_Get_InterferToolActNum(int* iNum, int comId);
IMC100API int FUNTYPE IMC100_Set_InterferToolPara(int iNum, S_INTERFER_TOOL* pstPara, int comId);
IMC100API int FUNTYPE IMC100_Get_InterferToolPara(int iNum, S_INTERFER_TOOL* pstPara, int comId);

IMC100API int FUNTYPE IMC100_Pulse(int donum, double dotime, int highflag, int comId);
IMC100API int FUNTYPE IMC100_Set_Acc(double accpercent, double decpercent, int comId);
IMC100API int FUNTYPE IMC100_Get_Acc(double *accpercent, double *decpercent, int comId);
IMC100API int FUNTYPE IMC100_Set_GripLoad(int griploadno, int comId);
IMC100API int FUNTYPE IMC100_Get_GripLoad(int *griploadno, int comId);


IMC100API int FUNTYPE IMC100_Set_DiagnosisItem(char cKey[10], int32_t i32Flag, int comId);
IMC100API int FUNTYPE IMC100_Get_DiagnosisItem(char cKey[10], int32_t *pi32Flag, int comId);
IMC100API int FUNTYPE IMC100_Set_DiagnosisSave(int comId);
IMC100API int FUNTYPE IMC100_Get_DiagnosisSave(int32_t *pi32Sts, int comId);
IMC100API int FUNTYPE IMC100_Get_OffsetRefnotJPos_RobP(ROB_POS stBasePos, POSE stPR, int ToolNum, int WobjNum, ROB_POS *pOutpos, int comId);
IMC100API int FUNTYPE IMC100_Get_OffsetRef_RobP(ROB_POS stBasePos, POSE stPR, int ToolNum, int WobjNum, ROB_JPOS stJPos, ROB_POS *pOutpos, int comId);
IMC100API int FUNTYPE IMC100_Set_OffSetMode(int iStatus, int comId);
IMC100API int FUNTYPE IMC100_Get_OffSetMode(int *piStatus, int comId);

#ifdef __cplusplus 
}
#endif

#endif
