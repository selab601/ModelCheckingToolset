# なにこれ
15nm726tの研究の実装

現状は"先行研究より便利だけど状況によってはくっそ遅い"

# 依存
丸括弧内は動作確認できたversion

+ Neo4j(3.0.7,3.1.0)

+ Boost(新しいの)

+ libneo4j-client(新しいの)

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

