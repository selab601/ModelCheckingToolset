# なにこれ
15nm726tの研究の実装

現状は"先行研究より便利だけど状況によってはくっそ遅い"

# 依存
丸括弧内は動作確認できたversion

+ Neo4j(3.0.7,3.1.0,3.1.1)

+ Boost(新しいの)

+ libneo4j-client(新しいの)(1.2.1)

いずれもbrew経由でinstallすればおｋ

    brew install neo4j
    brew install boost
    brew install cleishm/neo4j/neo4j-client


# ツールセットの使い方
Initial commit時点

    make
    ./bin/RuleSeparater [RuleList] [OutPutDir]
    ./generateState.sh [RuleList]_separated/ruleCuts_A [RuleList]_separated/ruleCuts_A_attrTable
    ./generateState.sh [RuleList]_separated/ruleCuts_B [RuleList]_separated/ruleCuts_B_attrTable
    ./bin/InitStateConverter [RuleList]_separated
    ./importDB.sh [RuleList]_separated
    neo4j restart(neo4jのアカウントのパスワードはneo4Jにする)
    ./bin/StateFinder ./[RuleList]_separated [開始状態] [終了状態]


モデルがcrane_2F_3の場合

    make
    
    #OutPutDir作成
    mkdir crane_2F_3

    #ルール分割
    ./bin/RuleSeparater models/crane_2F_3 models/crane_2F_3_attrTable crane_2F_3/ (←スラッシュ必須)
    
    #状態生成
    ./generateState.sh crane_2F_3/crane_2F_3_separated/ruleCuts_A crane_2F_3/crane_2F_3_separated/ruleCuts_A_attrTable
    ./generateState.sh crane_2F_3/crane_2F_3_separated/ruleCuts_B crane_2F_3/crane_2F_3_separated/ruleCuts_B_attrTable
    ./bin/InitStateConverter crane_2F_3_separated

    #自分のNeo4jのバージョンが3.0.7でない場合
    config ファイル上のパスを使用するバージョン名に変更する
    #データベース名をgraph.db以外にする場合
    neo4j/バージョン/libexec/conf/neo4j.conf を開き、#The name of the database to mountを変更
    
    #neo4jに遷移格納
    ./importDB.sh crane_2F_3/crane_2F_3_separated

    #neo4j再起動
    neo4j restart

    #遷移検索
    ./bin/StateFinder ./crane_2F_3/crane_2F_3_separated [開始状態][終了状態]

    #状態について
    属性一覧はcrane_3F_3_attrTableにある通り12種類
    A1,Loading,None         (荷物が格納されてる:0,格納されていない:1)
    A2,Loading,None
    B1,Loading,None
    B2,Loading,None
    C1,Loading,None
    C2,Loading,None
    CR1_LOADING,False,True  (クレーンが荷物を持っていない:0,持っている:1)
    CR1_X,A,B               (X座標がA:0,B:1)
    CR1_Y,_1,_2             (Y座標が1:0,2:1)
    CR2_LOADING,False,True
    CR2_X,B,C
    CR2_Y,_1,_2

    #すべての箱とクレーン上が空でクレーン１がA1,クレーン２がC1にいる場合
    1,1,1,1,1,1,0,0,0,0,1,0


# 当面の作業方針
branchを切る方針ともいう 上ほど高い

+ リファクタリング
    * 余裕のない人間の生み出したくそsyntaxを抹殺せよ

+ ツールセットをまたぐ概念の一元化
    * クラス化・共通ヘッダ化

    * 命名の統一

+ 機能実装・改良

# branchの運用方針
[GitHub Flow](https://gist.github.com/Gab-km/3705015)とかいう強いパーソンが考えた方針を適当に使います

+ master
    - 必ず動くもの

+ topic
    - 機能追加/修正/削除/リファクタリング/その他を全部これでやる
    - ex) hogeをfugaするbranch名→"hogeをfugaする" など

+ issue/プルリクの利用は適宜

