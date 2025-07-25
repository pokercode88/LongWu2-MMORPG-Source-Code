#include "Processor.h"
#include "globe.h"
#include "LogComm.h"
#include "DataProxyProto.h"
#include "ServiceDefine.h"
#include "util/tc_hash_fun.h"
#include "uuid.h"
#include "SocialServer.h"
#include "util/tc_base64.h"

//
using namespace std;
using namespace dataproxy;
using namespace dbagent;

//过期时间
#define GIFT_EXPIRED_TIME (24*60*60)

/**
 *
*/
Processor::Processor()
{
}

/**
 *
*/
Processor::~Processor()
{

}

// CHAT_EXT_INFO  = 49,    //扩展的聊天数据
//查询
int Processor::selectChatExtInfo(const string &number, TicketInfo &info)
{
    TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_STRING) + ":" + I2S(CHAT_EXT_INFO) + ":" + number;
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(number);

    vector<dbagent::TField> fields;
    dbagent::TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "content";
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TReadDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(number)->redisRead(dataReq, dataRsp);
    if (iRet != 0)
    {
        return -1;
    }

    ROLLLOG_DEBUG << "read cache, req: " << printTars(dataReq) << ", rsp: " << printTars(dataRsp) << endl;
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            //门票名称
            if (itfield->colName == "content")
            {
                __TRY__
                toObj(TC_Base64::decode(itfield->colValue), info);
                __CATCH__
            }
        }
    }

    return 0;
}

//添加
int Processor::addChatExtInfo(const string &number, const TicketInfo &info)
{
    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_STRING) + ":" + I2S(CHAT_EXT_INFO) + ":" + number;
    dataReq.operateType = E_REDIS_INSERT;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(number);
    // dataReq.paraExt.resetDefautlt();
    // dataReq.paraExt.queryType = E_REPLACE;

    vector<dbagent::TField> fields;
    dbagent::TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "transaction_id";
    tfield.colType = dbagent::STRING;
    tfield.colValue = number;
    fields.push_back(tfield);
    tfield.colName = "content";
    tfield.colType = dbagent::STRING;
    tfield.colValue = TC_Base64::encode(tostring(info));
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(number)->redisWrite(dataReq, dataRsp);
    ROLLLOG_DEBUG << "addChatExtInfo, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "addChatExtInfo err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    return 0;
}

//更新
int Processor::updateChatExtInfo(const string &number, const TicketInfo &info)
{
    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_STRING) + ":" + I2S(CHAT_EXT_INFO) + ":" + number;
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(number);

    vector<dbagent::TField> fields;
    dbagent::TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "transaction_id";
    tfield.colType = dbagent::STRING;
    tfield.colValue = number;
    fields.push_back(tfield);
    tfield.colName = "content";
    tfield.colType = dbagent::STRING;
    tfield.colValue = TC_Base64::encode(tostring(info));
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(number)->redisWrite(dataReq, dataRsp);
    ROLLLOG_DEBUG << "updateChatExtInfo, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "updateChatExtInfo err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    return 0;
}

int Processor::getGiveChipsTime(tars::Int64 uid, tars::Int64 friend_uid, int &give_time)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    if ((uid <= 0) || (friend_uid <= 0))
    {
        ROLLLOG_ERROR << "uid: " << uid << " or friend_uid: " << friend_uid << " error!" << endl;
        return -1;
    }

    dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(FRIEND_INFO) + ":" + L2S(uid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = uid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "friend_uid";
    fields.push_back(tfield);
    tfield.colName = "give_time";
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get friends give info err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -1;
    }

    give_time = 0;
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        long lFriendUid = 0;
        int iGiveTime = 0;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            if (itTField->colName == "friend_uid")
            {
                lFriendUid = S2L(itTField->colValue);
            }
            else if (itTField->colName == "give_time")
            {
                iGiveTime = g_app.getOuterFactoryPtr()->GetCustomTimeTick(itTField->colValue);
            }
        }
        ROLLLOG_DEBUG << "get friends give info, friend_uid: " << lFriendUid << ", iGiveTime: " << iGiveTime << endl;
        if ((lFriendUid > 0) && (iGiveTime > 0) && (lFriendUid == friend_uid))
        {
            give_time = iGiveTime;
            break;
        }
    }

    ROLLLOG_DEBUG << "====================== uid: " << uid << ", friend_uid: " << friend_uid << ", give_time: " << give_time << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

//USER_ACCOUNT = 20,     //#tbl_user_account
int Processor::selectUserAccount(const userinfo::GetUserReq &req, userinfo::GetUserResp &resp)
{
    if (req.uid <= 0)
    {
        ROLLLOG_ERROR << "invalid params, uid: " << req.uid << endl;
        resp.resultCode = -1;
        return -1;
    }

    TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(USER_ACCOUNT) + ":" + L2S(req.uid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    dataReq.clusterInfo.frageFactor = tars::hash<string>()(L2S(req.uid));

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "uid";
    fields.push_back(tfield);
    tfield.colName = "username";
    fields.push_back(tfield);
    tfield.colName = "password";
    fields.push_back(tfield);
    tfield.colName = "safes_password";
    fields.push_back(tfield);
    tfield.colName = "reg_type";
    fields.push_back(tfield);
    tfield.colName = "reg_time";
    fields.push_back(tfield);
    tfield.colName = "reg_ip";
    fields.push_back(tfield);
    tfield.colName = "reg_device_no";
    fields.push_back(tfield);
    tfield.colName = "is_robot";

    fields.push_back(tfield);
    tfield.colName = "agcid";
    fields.push_back(tfield);
    tfield.colName = "disabled";
    fields.push_back(tfield);
    tfield.colName = "device_id";
    fields.push_back(tfield);
    tfield.colName = "device_type";
    fields.push_back(tfield);
    tfield.colName = "platform";
    fields.push_back(tfield);
    tfield.colName = "channel_id";
    fields.push_back(tfield);
    tfield.colName = "area_id";
    fields.push_back(tfield);
    tfield.colName = "is_forbidden";
    fields.push_back(tfield);
    tfield.colName = "forbidden_time";
    fields.push_back(tfield);
    tfield.colName = "bindChannelId";
    fields.push_back(tfield);
    tfield.colName = "bindOpenId";
    fields.push_back(tfield);
    tfield.colName = "isinwhitelist";
    fields.push_back(tfield);
    tfield.colName = "whitelisttime";
    fields.push_back(tfield);
    tfield.colName = "country_id";
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TReadDataRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(req.uid)->redisRead(dataReq, dataRsp);
    if ((iRet != 0) || (dataRsp.iResult != 0))
    {
        ROLLLOG_ERROR << "get user-account failed, uid: " << req.uid << ", iResult: " << dataRsp.iResult << endl;
        resp.resultCode = -1;
        return -2;
    }

    if (dataRsp.fields.empty())
    {
        ROLLLOG_ERROR << "uid:" << req.uid << " not exist in tb_useraccount!" << endl;
        resp.resultCode = -1;
        return -3;
    }

    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        for (auto itfields = it->begin(); itfields != it->end(); ++itfields)
        {
            if (itfields->colName == "username")
            {
                resp.userName = itfields->colValue;
            }
            else if (itfields->colName == "device_id")
            {
                resp.deviceID = itfields->colValue;
            }
            else if (itfields->colName == "device_type")
            {
                resp.deviceType = itfields->colValue;
            }
            else if (itfields->colName == "platform")
            {
                resp.platform = (userinfo::E_Platform_Type)S2I(itfields->colValue);
            }
            else if (itfields->colName == "channel_id")
            {
                resp.channnelID = (userinfo::E_Channel_ID)S2I(itfields->colValue);
            }
            else if (itfields->colName == "area_id")
            {
                resp.areaID = S2I(itfields->colValue);
            }
            else if (itfields->colName == "is_robot")
            {
                resp.isRobot = S2I(itfields->colValue);
            }
            else if (itfields->colName == "reg_time")
            {
                resp.regTime = g_app.getOuterFactoryPtr()->GetCustomTimeTick(itfields->colValue);
            }
            else if (itfields->colName == "bindChannelId")
            {
                resp.bindChannelId = S2I(itfields->colValue);
            }
            else if (itfields->colName == "bindOpenId")
            {
                resp.bindOpenId = itfields->colValue;
            }
            else if (itfields->colName == "reg_type")
            {
                resp.regType = S2I(itfields->colValue);
            }
            else if (itfields->colName == "isinwhitelist")
            {
                resp.isinwhitelist = S2I(itfields->colValue);
            }
            else if (itfields->colName == "whitelisttime")
            {
                resp.whitelisttime = g_app.getOuterFactoryPtr()->GetCustomTimeTick(itfields->colValue);
            }
            else if(itfields->colName == "country_id")
            {
                resp.country_id = itfields->colValue;
            }
        }
    }

    ROLLLOG_DEBUG << "get user succ, userinfo::GetUserReq:" << printTars(req) << ", userinfo::GetUserResp:" << printTars(resp) << endl;
    resp.resultCode = 0;
    return 0;
}
//更改玩家备注
int Processor::ReplaceRemark(const long lUid, tars::Int64 remark_uid, std::string content, int state)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(USER_REMARK) + ":" + L2S(lUid);
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = lUid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(lUid);
    fields.push_back(tfield);
    tfield.colName = "remark_uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(remark_uid);
    fields.push_back(tfield);
    tfield.colName = "state";
    tfield.colType = INT;
    tfield.colValue = I2S(state);
    fields.push_back(tfield);
    tfield.colName = "content";
    tfield.colType = STRING;
    tfield.colValue = content;
    fields.push_back(tfield);
    tfield.colName = "log_time";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(TNOW);
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert remark err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert remark, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}

string Processor::getUserRemark(const long lUid, const long remark_uid)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__

   dataproxy::TReadDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(USER_REMARK) + ":" + L2S(lUid);
    dataReq.operateType = E_REDIS_READ;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = lUid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.subOperateType = E_REDIS_LIST_RANGE;//根据范围取数据
    dataReq.paraExt.start = 0;//起始下标从0开始
    dataReq.paraExt.end = -1;//终止最大结束下标为-1

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "remark_uid";
    fields.push_back(tfield);
    tfield.colName = "state";
    fields.push_back(tfield);
    tfield.colName = "content";
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TReadDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->redisRead(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "get user remark err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return "";
    }
    for (auto it = dataRsp.fields.begin(); it != dataRsp.fields.end(); ++it)
    {
        map<string, string> mapRow;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapRow.insert(std::make_pair(itTField->colName, itTField->colValue));
        }
        if(remark_uid == S2L(mapRow["remark_uid"]))
        {
            return mapRow["content"];
        }
    }

    __CATCH__
    FUNC_EXIT("", iRet);
    return "";
}

int Processor::addRemark(const long lUid, const FriendsProto::AddRemarkReq &req)
{
    return ReplaceRemark(lUid, req.remark_uid(), req.content(), 1);
}

int Processor::deleteRemark(const long lUid, const FriendsProto::DeleteRemarkReq &req)
{
    return ReplaceRemark(lUid, req.remark_uid(), "", 0);
}
string Processor::getRemarkContent(const long lUid, const long lRemarkID)
{
    FUNC_ENTRY("");
    __TRY__

    string table_name = "tb_remark";
    std::vector<string> col_name = { "remark_uid", "content"};
    std::vector<vector<string>> whlist = {
        {"uid", "0", L2S(lUid)},
        {"remark_uid", "0", L2S(lRemarkID)},
        {"state", "0", "1"}
    };

    dbagent::TDBReadRsp dataRsp;
    int iRet = readDataFromDBEx(lUid, table_name, col_name, whlist, "log_time", dataRsp);
    if(iRet != 0)
    {
        LOG_ERROR<<"select tb_remark err!"<< endl;
        return "";
    }

    for (auto it = dataRsp.records.begin(); it != dataRsp.records.end(); ++it)
    {
        map<string, string> mapRow;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapRow.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        return mapRow["content"];
    }

    __CATCH__
    FUNC_EXIT("", 0);
    return "";
}
int Processor::readDataFromDBEx(long uid, const string& table_name, const std::vector<string>& col_name, const std::vector<vector<string>>& whlist, const string& order_col, dbagent::TDBReadRsp &dataRsp)
{
    int iRet = 0;
    dbagent::TDBReadReq rDataReq;
    rDataReq.keyIndex = 0;
    rDataReq.queryType = dbagent::E_SELECT;
    rDataReq.tableName = table_name;

    vector<dbagent::TField> fields;
    dbagent::TField tfield;
    tfield.colArithType = E_NONE;
    for(auto item : col_name)
    {
        tfield.colName = item;
        fields.push_back(tfield);
    }
    rDataReq.fields = fields;

    //where条件组
    if(!whlist.empty())
    {
        vector<dbagent::ConditionGroup> conditionGroups;
        dbagent::ConditionGroup conditionGroup;
        conditionGroup.relation = dbagent::AND;
        vector<dbagent::Condition> conditions;
        for(auto item : whlist)
        {
            if(item.size() != 3)
            {
                continue;
            }
            dbagent::Condition condition;
            condition.condtion =  dbagent::Eum_Condition(S2I(item[1]));
            condition.colType = dbagent::STRING;
            condition.colName = item[0];
            condition.colValues = item[2];
            conditions.push_back(condition);
        }
        conditionGroup.condition = conditions;
        conditionGroups.push_back(conditionGroup);
        rDataReq.conditions = conditionGroups;
    }

    //order by字段
    if(!order_col.empty())
    {
        vector<dbagent::OrderBy> orderBys;
        dbagent::OrderBy orderBy;
        orderBy.sort = dbagent::DESC;
        orderBy.colName = order_col;
        orderBys.push_back(orderBy);
        rDataReq.orderbyCol = orderBys;
    }
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(uid)->read(rDataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "read data from dbagent failed, rDataReq:" << printTars(rDataReq) << ",dataRsp: " << printTars(dataRsp) << endl;
        return -1;
    }
    return 0;
}
int Processor::forbitChat(const long lUid, const FriendsProto::ForbitChatReq &req)
{
    FUNC_ENTRY("");
    int iRet = 0;
    __TRY__
    dataproxy::TWriteDataReq dataReq;
    dataReq.resetDefautlt();
    dataReq.keyName = I2S(E_REDIS_TYPE_LIST) + ":" + I2S(USER_FORBIT_CHAT) + ":" + L2S(lUid);
    dataReq.operateType = E_REDIS_WRITE;
    dataReq.clusterInfo.resetDefautlt();
    dataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    dataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_USER_ID;
    dataReq.clusterInfo.frageFactor = lUid;
    dataReq.paraExt.resetDefautlt();
    dataReq.paraExt.queryType = E_REPLACE;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(lUid);
    fields.push_back(tfield);
    tfield.colName = "forbit_uid";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(req.forbit_uid());
    fields.push_back(tfield);
    tfield.colName = "is_forbit";
    tfield.colType = INT;
    tfield.colValue = I2S(req.is_forbit());
    fields.push_back(tfield);
    tfield.colName = "log_time";
    tfield.colType = BIGINT;
    tfield.colValue = L2S(TNOW);
    fields.push_back(tfield);
    dataReq.fields = fields;

    dataproxy::TWriteDataRsp dataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(lUid)->redisWrite(dataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "insert forbit_uid err, iRet: " << iRet << ", iResult: " << dataRsp.iResult << endl;
        return -2;
    }

    ROLLLOG_DEBUG << "insert forbit_uid, iRet: " << iRet << ", dataRsp: " << printTars(dataRsp) << endl;

    __CATCH__
    FUNC_EXIT("", iRet);
    return iRet;
}
int Processor::getForbitChat(const long lUid, const long forbit_uid)
{
    FUNC_ENTRY("");
    __TRY__

    string table_name = "tb_forbitchat";
    std::vector<string> col_name = { "forbit_uid", "is_forbit"};
    std::vector<vector<string>> whlist = {
        {"uid", "0", L2S(lUid)},
        {"forbit_uid", "0", L2S(forbit_uid)}
    };

    dbagent::TDBReadRsp dataRsp;
    int iRet = readDataFromDBEx(lUid, table_name, col_name, whlist, "log_time", dataRsp);
    if(iRet != 0)
    {
        LOG_ERROR<<"select tb_forbitchat err!"<< endl;
        return 0;
    }

    for (auto it = dataRsp.records.begin(); it != dataRsp.records.end(); ++it)
    {
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            if (itfield->colName == "is_forbit")
            {
               return S2I(itfield->colValue);
            }
        }
    }

    __CATCH__
    FUNC_EXIT("", 0);
    return 0;
}