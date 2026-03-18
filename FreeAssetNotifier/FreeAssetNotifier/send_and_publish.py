import discord
import asyncio
import sys

# ✅ 설정 정보
TOKEN = 'TOKEN'
CHANNEL_ID = 1483645860935106793

async def run_bot(store, name, endDate, imgUrl, coupon):
    intents = discord.Intents.default()
    client = discord.Client(intents=intents)

    async with client:
        await client.login(TOKEN)
        channel = client.get_channel(CHANNEL_ID) or await client.fetch_channel(CHANNEL_ID)

        if channel:
            # ✅ 인자로 받은 store 값에 따라 색상만 결정
            embed_color = 0x223696 if "Unity" in store else 0x2ecb71

            embed = discord.Embed(
                title="🔔 **새로운 무료 에셋!**",
                description=f"**[{name}]**", 
                color=embed_color
            )
            embed.add_field(name="🎁 쿠폰 코드", value=f"`{coupon}`", inline=True)
            embed.add_field(name="🛒 스토어", value=store, inline=True)
            embed.add_field(name="⏰ 종료 예정일", value=f"{endDate}", inline=False)
            
            if imgUrl and imgUrl != "N/A":
                embed.set_image(url=imgUrl)
            
            try:
                message = await channel.send(embed=embed)
                print(f"Message sent to {store}!")

                if channel.type == discord.ChannelType.news:
                    try:
                        await asyncio.wait_for(message.publish(), timeout=15.0)
                        print("Successfully published to announcement channel!")
                    except Exception as e:
                        print(f"Publish Notice: {e}")
                
            except Exception as e:
                print(f"Send Error: {e}")
                sys.exit(1)
        
        await client.close()

if __name__ == "__main__":
    # ✅ 인자가 5개인지 확인 (store, name, date, img, coupon)
    if len(sys.argv) < 6:
        print("Error: Missing arguments (Expected 5)")
        sys.exit(1)
    
    # 인자 순서: [1]store, [2]name, [3]date, [4]img, [5]coupon
    store, name, date, img, coupon = sys.argv[1:6]
    
    try:
        asyncio.run(asyncio.wait_for(run_bot(store, name, date, img, coupon), timeout=45.0))
    except Exception as e:
        # 정상 종료 시 CancelledError가 뜰 수 있으나 무시해도 무방
        if "CancelledError" not in str(e):
            print(f"Runtime Info: {e}")