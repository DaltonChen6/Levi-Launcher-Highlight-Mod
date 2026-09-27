#include "render/FontManager.h"
#include "render/EmbeddedFont.h"

#include <android/log.h>
#include <cstdio>

namespace highlight {

void FontManager::setupFont(ImGuiIO &io) {
  static ImVector<ImWchar> sChineseRanges;
  if (sChineseRanges.empty()) {
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddText(
        "√×✔✘【】[]()（）:：|%米秒度格阶/ID0123456789.-+*~❤♥★☆◆◇▲▼►◄●○ "
        "预计待机中瞬破已用空容器对准打开后自动记录未同步"
        "速度跳跃生命护甲耐久项圈职业等级阶段傀儡裂纹怒气拾取品质神话传奇史诗稀有普通罕见监守者幼年成年身体状态完好轻微严重破损颜色"
        "一丁三上下不与专且世丘丛丝丢严中串丹为主丽举义之乐乙书乳了二于云五井亚些亡交亮人仅从他仙代令以件任份休优会传伦伯位低体何余作你佳使例供侦保信修值偏停傀储催像僵儡"
        "允元充先光克免兔兜入全公六兰兵其具典兼兽内冰冲准凋凝出函刀刃分切则创初判利别刮到制刷刺刻剂削前剑剩剪割力加动助劫包化北匙匹区匿十升半华单南卜占卡卫危即卵厄压原去产"
        "叉及友双反发取受变叠口古只叫可台史右叶号合名后向否含呈告员周命和咸哀品响哞哨哭唇唤唱商啸善嗅嘎嘴器嚎囊回因团园围图圆土在地坏坐块垂型垫埋城基堆塑塔塞填墙墨壁士声"
        "壳处备复大天太夫失头夺奇女奶如姆始娜子字存孢守安完官定宝实家容富对寻导射将小尔尖尝尸尾局层居屏展属山岗岩岸峭崩嵌川巢工左巫已巴布帜带帧常帽幕干平年并幻幼幽庄床库"
        "应底废度建开异弃式弓张弩弹强当录形彩影径徒得循徽心必快念态怕思性怨怪恩息恶恼悦您悬悼想感慈戏成或戟户所扇手才打执扫找把投折护报拉拒拟拢括拴拼持挂指按挖挚挡挽捆损"
        "换据掉掌排掘掠探推掩掷描提搏摄撞操支收改放效数文斑斗料斜斧断方旋旗无日时明星是显晚晶暂暖暗曜曲更最有望期木未末本朱朵机权杆杉材村杜条来杨板极构析林果枝枯架柄染查"
        "柱栅标栏树校株样核根格框案桦桶梦梯检棍棒棕森植楼模横樱橙橡次欢欣正此武死残殖段母毒比毛毯民气氖氙氟氡氢氦氧氨氩氪氮氯水永汇汉汐汞汤沉沙河油沼沾法泡波泣泥泪泽洋洞"
        "活派流浅浆测浪浮海涂涌涡液淡深混添清渐渲渴游湿溃源溯溴溺滑满滨滴漂漏漠潜潮激灌火灭灯灰灵灾炉炖炬炭炸点炼炽烁烈烛烟烤烧热烯烹焦焰煤照煲熄熊熏熔熟燃燧爆爱片版牌牛"
        "牡牧物特犰状狐狳狸狼猪猫玄玩玫环现玻珀珊珍珠球理瑚瑰璃瓜瓣瓦瓶瓷甘甜生用甲画界留疑疣疫白皂的皮盆盐监盒盔盛目盲相盾看真眸眼睡瞄矛矢知短矮石矿砂砖砧破砷砹砾硅硒硝"
        "硫确硼碎碗碘碰碱碲碳磁磨磷示祥离种秒称移稳穴空立竖竹竿笋笔笛符笼等筏签简算管箭箱簇簿籍米类粉粒粗粘精糕糖系素索紫繁红级纯纳纸纹线组细织终绊绒结络绝统继绯绳绵绽绿"
        "缀缓编缠缪罐网罗置羊美羽翅翠翻翼考者而耕耳聊聚肉肋肥背胡胸能脂脉脑脚脱脾腐腾腿膏膜臂自至船色花芽苍苔苗若苦苯英苹范茎草荐荒荚荡荧荫药莉莱莲获菇菊菌菜菱萎萝萤营落"
        "葱葵蒂蒲蓝蔓蔗蕨薯藏藓藤蘑虞虫蚀蚪蛋蛙蛛蛮蜂蜘蜜蜡蝌蝙蝠蝴蝶螈螨螺蠹行衣补袋被袭裂装裤西要视觉角解计认议讲许设证识词译试诡该误诺读调谨豆豚豹贝质赋赏赛赭走超足跃"
        "距路踪车轨转轮轻载较辅辑输边过运近返还这进远述迷迹追退送适逆选通速造逻道遗遮避郁部都配酵酸酿采釉重野量金鈇鉝鉨錀錾鎶鐽鑪钆钇针钋钌钍钐钒钓钔钕钙钚钛钟钠钡钥钨钩"
        "钪钫钬钮钯钴钷钻钼钽钾铀铁铂铃铅铆铈铊铋铌铍铑铒铕铜铝铟铠铥铪铬铯铱银铷铹铼铽链销锁锂锄锅锆锇锈锌锎锑锔锕锗锘错锚锝锡锤锥锫锭键锯锰锶锹锻锿镁镄镅镆镉镍镎镐镓镜"
        "镝镤镥镧镨镭镰镱镶长门闪间阱防阳阶阻阿附限除险陶陷隐隙障雀集雇雕雨雪零雷需霜青静非靠面革靴靶鞍鞘音顶须颂颅额风飞食饪饰饶首香马驳驴驼骆验骡骨骷骸髅高鬼魂魔鱼鱿鲀"
        "鲁鲅鲑鲷鳕鳚鳞鸡鸭鹃鹉鹦鹿麦黄黏黑默鼻齐龙龟");
    builder.BuildRanges(&sChineseRanges);
  }

  // 1. 优先使用内置 TTF 字体
  if (kEmbeddedFontDataSize > 0) {
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;

    ImFont *font = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char *>(kEmbeddedFontData),
        static_cast<int>(kEmbeddedFontDataSize), 28.0f, &cfg,
        sChineseRanges.Data);
    if (font != nullptr) {
      __android_log_print(ANDROID_LOG_INFO, "VH",
                          "Successfully loaded embedded Chinese font (%zu bytes)",
                          kEmbeddedFontDataSize);
      return;
    }
  }

  // 2. 备选方案：系统字体加载
  const char *kFontPaths[] = {
      "/system/fonts/DroidSansFallback.ttf",
      "/system/fonts/NotoSansSC-Regular.otf",
      "/system/fonts/NotoSansHans-Regular.otf",
      "/system/fonts/NotoSansCJK-Regular.ttc",
      "/system/fonts/Roboto-Regular.ttf"
  };

  for (const char *path : kFontPaths) {
    FILE *f = std::fopen(path, "rb");
    if (!f) continue;
    std::fclose(f);

    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;

    ImFont *font = io.Fonts->AddFontFromFileTTF(path, 28.0f, &cfg, sChineseRanges.Data);
    if (font != nullptr) {
      __android_log_print(ANDROID_LOG_INFO, "VH", "Loaded font from %s", path);
      return;
    }
  }

  io.Fonts->AddFontDefault();
}

} // namespace highlight
