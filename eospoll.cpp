/**
 * *  The apply() methods must have C calling convention so that the blockchain can lookup and
 * *  call these methods.
 * */
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <string>

extern "C" {
	struct transferargs {
	   account_name      from;
	   account_name      to;
	   eosio::asset      quantity;
	   std::string       memo;
	};

	struct revealargs {
	   uint64_t          tip;
	};

	struct globalindex {
	   uint64_t          id;
	   uint64_t          gindex;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(globalindex), globalindex> globalindex_index;

	struct betplayer {
		account_name      user;
		account_name      proxyer;
		account_name primary_key() const { return user; }
	};
	typedef eosio::multi_index< N(betplayer), betplayer> betplayer_index;

	struct betstate {
	   uint64_t          round;
	   uint64_t          counter;
	   uint64_t          total;
	   eosio::asset      quantity;
	   eosio::asset      proxy;
	   uint64_t          result;
	   uint64_t primary_key()const { return round; }
	};
	typedef eosio::multi_index< N(betstate), betstate> betstate_index;

    struct betoffer {
    	uint64_t         id;
    	account_name     from;
    	account_name     to;
    	eosio::asset     quantity;
    	std::string      memo;
    	uint64_t primary_key() const {return id;}
    };
    typedef eosio::multi_index<N(betoffer), betoffer> betoffer_index;

	struct betnumber {
	   uint64_t          id;
	   uint64_t          offerbetid;
	   uint64_t          number;
	   uint64_t primary_key()const { return id; }
	};
	typedef eosio::multi_index< N(betnumber), betnumber> betnumber_index;

	struct betledger {
		account_name      user;
		eosio::asset      quantity;
		account_name primary_key() const {return user;}
	};
	typedef eosio::multi_index< N(betledger), betledger> betledger_index;

   /// The apply method implements the dispatch of events to this contract
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {

	 //
	 auto _self = receiver;

	 //
	 globalindex_index globalindexs(_self, _self);
	 auto cur_globalindex_itr = globalindexs.find(N(bet));
	 if(cur_globalindex_itr == globalindexs.end()) {
		 cur_globalindex_itr = globalindexs.emplace(_self, [&](auto& info){
			 info.id       = N(bet);
			 info.gindex  = 0;
		 });
	 }

	 //
	 uint64_t bet_index = cur_globalindex_itr->gindex;

	 //
	 betstate_index betstates(_self, _self);
	 auto cur_betstate_itr = betstates.find(bet_index);
	 if(cur_betstate_itr == betstates.end()) {
		 cur_betstate_itr = betstates.emplace(_self, [&](auto& info){
			 info.round   = bet_index;
			 info.total   = 0;
			 info.result  = 0xFFFFFFFFFFFFFFFF;
		 });
	 }

	 //
	 uint64_t bet_total = cur_betstate_itr->total;
	 eosio::asset bet_quantity = cur_betstate_itr->quantity;
	 eosio::asset bet_proxy = cur_betstate_itr->proxy;

	 //
	 betoffer_index betoffers(_self, _self);
	 betnumber_index betnumbers(_self, _self);
	 betplayer_index betplayers(_self, _self);
	 betledger_index betledgers(_self, _self);

	 //
	 if(code == N(eosio.token) && action == N(transfer)) {
		 transferargs args = eosio::unpack_action_data<transferargs>();
		 if(args.to != _self) {
			 return;
		 }

		auto sym = args.quantity.symbol;
		eosio_assert( sym.is_valid(), "invalid symbol name" );
		eosio_assert( sym == CORE_SYMBOL, "invalid symbol name" );

		eosio_assert( args.memo.size() <= 256, "memo has more than 256 bytes" );
	    eosio_assert( args.quantity.is_valid(), "invalid quantity" );
	    eosio_assert( args.quantity.amount > 0, "must issue positive quantity" );

		 auto new_betoffer_itr = betoffers.emplace(_self, [&](auto& info){
			info.id           = betoffers.available_primary_key();
			info.from         = args.from;
			info.to           = args.to;
			info.quantity     = args.quantity;
			info.memo         = args.memo;
		 });

		 // user has register??
		 auto cur_betplayer_itr = betplayers.find(args.from);
		 if(cur_betplayer_itr == betplayers.end()) {

			 // get the proxyer
			 account_name proxyer = eosio::string_to_name(args.memo);
			 auto proxy_itr = betplayers.find(proxyer);
			 if(proxy_itr == betplayers.end()) {
				 proxyer = N(none);
			 }

			 //
			 cur_betplayer_itr = betplayers.emplace(_self, [&](auto& info){
				 info.user = args.from;
				 info.proxyer = proxyer;
			 });
		 }

		 // get code
		 eosio::asset temp;
		 temp.set_amount(10000);
		 eosio::asset amount;

		 while(args.quantity >= temp) {
			 args.quantity = args.quantity - temp;

			 auto new_betnumber_itr = betnumbers.emplace(_self, [&](auto& info){
				info.id           = betnumbers.available_primary_key();
				info.offerbetid   = new_betoffer_itr->id;
				info.number       = info.id;
			 });

			 //
			 bet_total ++;
			 bet_quantity += temp;
			 amount += temp;
		 }

		 // update ledger
		 do {
			 account_name proxyer1 = cur_betplayer_itr->proxyer;
			 if(proxyer1 == N(none)) {
				 break;
			 }

			 //
			 auto proxyer1_betledger_itr = betledgers.find(proxyer1);
			 if(proxyer1_betledger_itr == betledgers.end()) {
				 proxyer1_betledger_itr = betledgers.emplace(_self, [&](auto& info) {
					 info.user       = proxyer1;
					 info.quantity   = amount/10;
				 });
			 } else {
				 //
				 betledgers.modify( proxyer1_betledger_itr, 0, [&](auto& info) {
					 info.quantity   += amount/10;
				 });
			 }
			 bet_proxy += amount/10;

			 //
			 auto proxyer1_betplayer_itr = betplayers.find(proxyer1);
			 account_name proxyer2 = proxyer1_betplayer_itr->proxyer;
			 if(proxyer2 == N(none)) {
				 break;
			 }

			 //
			 auto proxyer2_betledger_itr = betledgers.find(proxyer2);
			 if(proxyer2_betledger_itr == betledgers.end()) {
				 proxyer2_betledger_itr = betledgers.emplace(_self, [&](auto& info) {
					 info.user       = proxyer2;
					 info.quantity   = (amount*5)/100;
				 });
			 } else {
				 //
				 betledgers.modify( proxyer2_betledger_itr, 0, [&](auto& info) {
					 info.quantity   += (amount*5)/100;
				 });
			 }
			 bet_proxy += (amount*5)/100;


			 //
			 auto proxyer2_betplayer_itr = betplayers.find(proxyer2);
			 account_name proxyer3 = proxyer2_betplayer_itr->proxyer;
			 if(proxyer3 == N(none)) {
				 break;
			 }

			 //
			 auto proxyer3_betledger_itr = betledgers.find(proxyer3);
			 if(proxyer3_betledger_itr == betledgers.end()) {
				 proxyer3_betledger_itr = betledgers.emplace(_self, [&](auto& info) {
					 info.user       = proxyer3;
					 info.quantity   = (amount*3)/100;
				 });
			 } else {
				 //
				 betledgers.modify( proxyer3_betledger_itr, 0, [&](auto& info) {
					 info.quantity   += (amount*3)/100;
				 });
			 }
			 bet_proxy += (amount*3)/100;
		 } while(false);

		 // update bet state
		 betstates.modify( cur_betstate_itr, 0, [&](auto& info) {
			 info.total = bet_total;
			 info.quantity = bet_quantity;
			 info.proxy = bet_proxy;
			 info.counter ++;
		 });
	 }

	 //
	 if(code == N(eospool) && action == N(reveal)) {
		 revealargs args = eosio::unpack_action_data<revealargs>();

		 //
         uint32_t t_now = now();
         checksum256 hash;
         sha256((char*)(&t_now), sizeof(uint32_t), &hash);
         uint64_t bet_result = hash.hash[1];
         bet_result = bet_result << 32;
         bet_result |= hash.hash[0];
         bet_result = bet_result % cur_betstate_itr->total;
         //uint64_t bet_result = N(hash.hash);
         //bet_result = bet_result % cur_betstate_itr->total;
         //uint64_t bet_result = (hash.hash[15]) % cur_betstate_itr->total;

         //
		 betstates.modify( cur_betstate_itr, 0, [&](auto& info) {
			 info.result = bet_result;
		 });

         // prize
         auto prize_betnumber_itr = betnumbers.find(bet_result);
         auto prize_betoffer_itr = betoffers.find(prize_betnumber_itr->offerbetid);
		 eosio::asset prize_total = cur_betstate_itr->quantity - cur_betstate_itr->proxy;
		 prize_total = (prize_total*9)/10;
		 account_name prize_account = prize_betoffer_itr->from;
		 auto prize_betledger_itr = betledgers.find(prize_account);
		 if(prize_betledger_itr == betledgers.end()) {
			 prize_betledger_itr = betledgers.emplace(_self, [&](auto& info) {
				 info.user       = prize_account;
				 info.quantity   = prize_total;
			 });
		 } else {
			 //
			 betledgers.modify( prize_betledger_itr, 0, [&](auto& info) {
				 info.quantity   += prize_total;
			 });
		 }

		 // transfer
		 auto betledger_itr = betledgers.begin();
		 while(betledger_itr != betledgers.end()) {
	         eosio::action(
	            eosio::permission_level{ _self, N(active) },
	            N(eosio.token), N(transfer),
	            std::make_tuple(_self, betledger_itr->user, betledger_itr->quantity, std::string("bonus"))
	         ).send();

	         betledger_itr ++;
		 }

		 // next round
         bet_index ++;
         globalindexs.modify(cur_globalindex_itr, 0, [&](auto& info) {
        	 info.gindex = bet_index;
         });

         {
        	 betoffer_index betoffers(_self, _self);
			  auto betoffer_itr = betoffers.begin();
			  while(betoffer_itr != betoffers.end()) {
				  betoffer_itr = betoffers.erase(betoffer_itr);
			  }
		  }

		  {
			  betnumber_index betnumbers(_self, _self);
			  auto betnumber_itr = betnumbers.begin();
			  while(betnumber_itr != betnumbers.end()) {
				  betnumber_itr = betnumbers.erase(betnumber_itr);
			  }
		  }

	         //
	         betledger_index betledgers(_self, _self);
			 auto betledger_itr = betledgers.begin();
			 while(betledger_itr != betledgers.end()) {
				 betledger_itr = betledgers.erase(betledger_itr);
			 }

	 }

	 //
     if(code == N(eospool) && action == N(reset)) {
    	 {
    		 betoffer betoffers(_self, _self);
			 auto betoffer_itr = betoffers.begin();
			 while(betoffer_itr != betoffers.end()) {
				 betoffer_itr = betoffers.erase(betoffer_itr);
			 }
    	 }

    	 {
    		 betnumber_index betnumbers(_self, _self);
			 auto betnumber_itr = betnumbers.begin();
			 while(betnumber_itr != betnumbers.end()) {
				 betnumber_itr = betnumbers.erase(betnumber_itr);
			 }
    	 }

    	 {
    		 globalindex_index globalindexs(_self, _self);
			 auto globalindex_itr = globalindexs.begin();
			 while(globalindex_itr != globalindexs.end()) {
				 globalindex_itr = globalindexs.erase(globalindex_itr);
			 }
    	 }

    	 {
    		 betstate_index betstates(_self, _self);
			 auto betstate_itr = betstates.begin();
			 while(betstate_itr != betstates.end()) {
				 betstate_itr = betstates.erase(betstate_itr);
			 }
    	 }

    	 {
    		 betplayer_index betplayers(_self, _self);
			 auto betplayer_itr = betplayers.begin();
			 while(betplayer_itr != betplayers.end()) {
				 betplayer_itr = betplayers.erase(betplayer_itr);
			 }
    	 }

    	 {
    		 betledger_index betledgers(_self, _self);
			 auto betledger_itr = betledgers.begin();
			 while(betledger_itr != betledgers.end()) {
				 betledger_itr = betledgers.erase(betledger_itr);
			 }
    	 }
     }
   }
} // extern "C"
